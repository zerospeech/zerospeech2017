#!/usr/bin/env python
# -*- coding: utf-8 -*-

# ------------------------------------
# file: features.py
# date: Tue April 22 18:31 2014
# author:
# Maarten Versteegh
# github.com/mwv
# maartenversteegh AT gmail DOT com
#
# Licensed under GPLv3
# ------------------------------------
"""features:

"""

from __future__ import division

import argparse
import glob
import json
import logging
import numpy as np
import os.path as path
import os
import scipy.io
import shutil
import spectral
import struct
import sys
import tempfile
import wave

import npz2h5features


def resample(sig, ratio):
    try:
        import scikits.samplerate
        return scikits.samplerate.resample(sig, ratio, 'sinc_best')
    except ImportError:
        import scipy.signal
        return scipy.signal.resample(sig, int(round(sig.shape[0] * ratio)))


def parse_args(args=sys.argv):
    parser = argparse.ArgumentParser(
        prog='features.py',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description='Extract Mel spectral features from audio files.',
        epilog="""Example usage:

$ python features.py test/wavs/*.wav -c mel_config.json

extracts features from audio files in current directory.\n

The output format is binary .npz. Each file contains two arrays, one holding
the features, the other the center times (in seconds) of the frames. To load
these files in python:

>>> import numpy as np
>>> data = np.load('/path/to/file.npz')
>>> features = data['features']
>>> center_times = data['time']
""")

    parser.add_argument('files', metavar='WAV',
                        nargs='+',
                        help='path to input audio files')

    parser.add_argument('-h5',
                        action='store',
                        dest='h5_output',
                        required=False,
                        help='output file in h5 format.\n'
                             ' This is the default output format')

    parser.add_argument('-csv', 
                        action='store',                            
                        dest='csv_output',                          
                        required=False,                            
                        help='output file in csv format.\n') 


    parser.add_argument('-npz',
                        action='store',
                        dest='npz_output',
                        required=False,
                        help='output directory in npz format.\n'
                             'only precise it if you want to use the numpy '
                             'matrices directly')

    parser.add_argument('-mat',
                        action='store',
                        dest='mat_output',
                        required=False,
                        help='output directory in matlab format.\n'
                             'Only precise it if you want to use the matlab '
                             'matrices directly')

    parser.add_argument('-c', '--config',
                        action='store',
                        dest='config',
                        required=True,
                        help='configuration file.\n'
                             'Contains the type of features to extract and the'
                             ' parameters in json format. See the default '
                             'config files for examples:\n'
                             'mel_config.json, mfcc_config.json, '
                             'rasta_config.json, lyon_config.json, '
                             'drnl_config.json')

    parser.add_argument('-f', '--force',
                        action='store_true',
                        dest='force',
                        default=False,
                        help='force resampling in case of samplerate mismatch.'
                             '\nUse this option with parcimony, only to '
                             'make the samplerate uniform among the files. '
                             'If you want to resample every file, you should '
                             'do it before running the program.')

    parser.add_argument('-v', '--verbose',
                        action='store_true',
                        help='display some log messages to stdout')

    parser.add_argument('--tempdir', default='/tmp',
                        help='directory where to write temp files (erased after execution). '
                        'Default is to write to /tmp')
    return vars(parser.parse_args(args))


def convert(files, outdir, encoder, force, verbose=False):
    if verbose:
        print 'computing features for {} wav files'.format(len(files))
    for n, f in enumerate(files):
        try:
            fid = wave.open(f, 'r')
            _, _, fs, nframes, _, _ = fid.getparams()
            sig = np.array(struct.unpack_from("%dh" % nframes,
                                              fid.readframes(nframes)))
            fid.close()
        except IOError:
            print('No such file:', f)
            exit()

        if fs != encoder.config['fs']:
            if force:
                sig = resample(sig, encoder.config['fs'] / fs)
            else:
                print ('Samplerate mismatch, expected {0}, got {1}, in {2}.\n'
                       'Use option -f to force resampling of the audio file. '
                       'Note that you should use force with parsimony, it is '
                       'better to adjust the sampling rate to your wav files'
                       .format(encoder.config['fs'], fs, f))
                exit()

        feats_tmp = encoder.transform(sig) 
        feats = feats_tmp[0]
        for i in range(1, len(feats_tmp)): feats = np.hstack((feats, feats_tmp[i]))

        wshift_smp = encoder.config['fs'] / encoder.config['frate']
        wlen_smp = encoder.config['wlen'] * encoder.config['fs']
        nframes = int(sig.shape[0] / wshift_smp + 1)
        if nframes != feats.shape[0]:
            raise ValueError('nframes mismatch. expected {0}, got {1}'
                             .format(feats.shape[0], nframes))

        center_times = np.zeros((nframes,))
        for fr in range(nframes):
            start_smp = round(fr * wshift_smp)
            end_smp = min(sig.shape[0], start_smp + wlen_smp)
            start_ms = start_smp / fs
            end_ms = end_smp / fs
            center_times[fr] = (start_ms + end_ms) / 2

        bname = path.splitext(path.basename(f))[0]
        np.savez(path.join(outdir, bname + '.npz'),
                 features=feats,
                 time=center_times)

        if verbose:
            print 'done {}/{} : {}'.format(n+1, len(files), bname)


def center_times(fs, wshift_smp, wlen_smp, sig_len):
    nframes = int(sig_len / wshift_smp + 1)
    center_times = np.zeros((nframes,))
    for fr in range(nframes):
        start_smp = round(fr * wshift_smp)
        end_smp = min(sig_len,
                      start_smp + wlen_smp)
        start_ms = start_smp / fs
        end_ms = end_smp / fs
        center_times[fr] = (start_ms + end_ms) / 2


def mat2npz(indir, outdir):
    for f in glob.glob(indir + '*.mat'):
        mat = scipy.io.loadmat(f)
        np.savez(path.join(outdir, os.path.basename(f) + '.npz'),
                 features=mat['features'],
                 time=np.ravel(mat['center_times']))


def main(args=sys.argv):
    args = parse_args(args)
    config_file = args['config']
    try:
        with open(config_file, 'r') as fid:
            config = json.load(fid)
    except IOError:
        print('No such file: ', config_file)
        exit()

    force = args['force']
    feat = config['features']
    h5file = args['h5_output']
    csvfile = args['csv_output']
    npzdir = args['npz_output']
    matdir = args['mat_output']
    files = args['files']
    verbose = args['verbose']
    tempdir = args['tempdir']

    ## TODO strange bug from parsing where files[0] == __file__
    if files[0][-3:] == '.py':
        del files[0]
    path_to_files=files[0]
    list_of_files=[os.path.join(path_to_files,f) for f in os.listdir(path_to_files) if os.path.isfile(os.path.join(path_to_files,f)) and f.endswith('.wav')]
    # print('\n'.join(files))

    if not (h5file or csvfile or npzdir or matdir):
        h5file = feat + '.features'

    if h5file and os.path.exists(h5file):
        print('The output file already exists: {}'.format(h5file))
        exit()
    if csvfile and os.path.exists(csvfile):
        print('The output file already exists: {}'.format(csvfile)) 
        exit()
    if npzdir and not os.path.exists(npzdir):
        print('No such directory: {}'.format(npzdir))
        exit()
    if matdir and not os.path.exists(matdir):
        print('No such directory: {}'.format(matdir))
        exit()

    tmp = False
    python = (feat == 'mel' or feat == 'mfcc')
    try:
        if (python and not npzdir):
            outdir = tempfile.mkdtemp(dir=tempdir)
            tmp = True
            if verbose:
                print 'writing to tempdir {}'.format(outdir)

        elif python:
            outdir = npzdir
        elif matdir:
            outdir = matdir

        if feat == 'mel':
            del config['features']
            encoder = spectral.CubicMel(**config)
            convert(list_of_files, outdir, encoder, force, verbose)
        elif feat == 'mfcc':
            del config['features']
            encoder = spectral.MFCC(**config)
            convert(list_of_files, outdir, encoder, force, verbose)
        if h5file:
            npz2h5features.convert(outdir, h5file)



    finally:
        if tmp:
            shutil.rmtree(outdir)


if __name__ == '__main__':
    main()
