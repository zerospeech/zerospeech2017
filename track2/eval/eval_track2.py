#!/usr/bin/env python
#
# Copyright 2015, 2017 Maarten Versteegh, Bogdan Ludusan, Juan Benjumea
# 
# You can redistribute this file and/or modify it under the terms of
# the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any  
# later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
"""Evaluation program for the Zero Speech Challenge 2017

Track 2: Spoken Term Discovery

"""                                                                     

from __future__ import division

import os
import os.path as path
import sys
from itertools import izip

import numpy as np
from joblib import Parallel, delayed

import tde.reader
import tde.util
import tde.token_type
import tde.corpus
import tde.match
import tde.nlp
import tde.group
import tde.boundaries

VERSION = "0.1.0"

def _match_sub(disc_clsdict, gold_clsdict, phn_corpus, names, label,
               verbose, n_jobs):
    em = tde.match.eval_from_psets
    if verbose:
        print '  matching ({2}): subsampled {0} files in {1} sets'\
            .format(sum(map(len, names)), len(names), label)
    with tde.util.verb_print('  matching ({0}): prepping psets'.format(label),
                             verbose, True, True, True):
        pdiscs = [tde.match.make_pdisc(disc_clsdict.restrict(ns, True),
                                       False, False)
                  for ns in names_cross]
        pgolds = [tde.match.make_pgold(gold_clsdict.restrict(ns, True),
                                       False, False)
                  for ns in names_cross]
        psubs = [tde.match.make_psubs(disc_clsdict.restrict(ns, True),
                                      phn_corpus, 3, 20, False, False)
                 for ns in names_cross]
    with tde.util.verb_print('  matching ({0}): calculating scores'
                             .format(label), verbose, False, True, False):
        tp, tr = izip(*Parallel(n_jobs=n_jobs,
                                verbose=5 if verbose else 0,
                                pre_dispatch='n_jobs')
                      (delayed(em)(pdisc, pgold, psub)
                      for pdisc, pgold, psub in zip(pdiscs, pgolds, psubs)))
    return np.fromiter(tp, dtype=np.double), np.fromiter(tr, dtype=np.double)

def match(disc_clsdict, gold_clsdict, phn_corpus, names_within, names_cross,
          dest, verbose, n_jobs):
    if verbose:
        print tde.util.dbg_banner('MATCHING')
    pc, rc = _match_sub(disc_clsdict, gold_clsdict, phn_corpus, names_cross,
                        'cross', verbose, n_jobs)
    fc = np.vectorize(tde.util.fscore)(pc, rc)

    pw, rw = _match_sub(disc_clsdict, gold_clsdict, phn_corpus, names_within,
                        'within', verbose, n_jobs)
    fw = np.vectorize(tde.util.fscore)(pw, rw)
    with open(path.join(dest, 'matching'), 'w') as fid:
        fid.write(tde.util.pretty_scores(pc, rc, fc, 'match cross-speaker',
                                         len(names_cross),
                                         sum(map(len, names_cross))))
        fid.write('\n')
        fid.write(tde.util.pretty_scores(pw, rw, fw, 'match within-speaker',
                                         len(names_within),
                                         sum(map(len, names_within))))

def _group_sub(disc_clsdict, names, label, verbose, n_jobs):
    eg = tde.group.evaluate_group
    if verbose:
        print '  group ({2}): subsampled {0} files in {1} sets'\
            .format(sum(map(len, names)), len(names), label)
    with tde.util.verb_print('  group ({0}): calculating scores'.format(label),
                             verbose, False, True, False):
        p, r = izip(*(Parallel(n_jobs=n_jobs,
                              verbose=5 if verbose else 0,
                              pre_dispatch='n_jobs')
                     (delayed(eg)(disc_clsdict.restrict(ns, True))
                      for ns in names)))
    return np.fromiter(p, dtype=np.double), np.fromiter(r, dtype=np.double)

def group(disc_clsdict, names_within, names_cross, dest, verbose, n_jobs):
    if verbose:
        print tde.util.dbg_banner('GROUP')
    pc, rc = _group_sub(disc_clsdict, names_cross, 'cross', verbose, n_jobs)
    fc = np.vectorize(tde.util.fscore)(pc, rc)

    pw, rw = _group_sub(disc_clsdict, names_within, 'within', verbose, n_jobs)
    fw = np.vectorize(tde.util.fscore)(pw, rw)
    with open(path.join(dest, 'group'), 'w') as fid:
        fid.write(tde.util.pretty_scores(pc, rc, fc, 'group cross-speaker',
                                         len(names_cross),
                                         sum(map(len, names_cross))))
        fid.write('\n')
        fid.write(tde.util.pretty_scores(pw, rw, fw, 'group within-speaker',
                                         len(names_within),
                                         sum(map(len, names_within))))

def _token_type_sub(clsdict, wrd_corpus, names, label, verbose, n_jobs):
    et = tde.token_type.evaluate_token_type
    if verbose:
        print '  token/type ({2}): subsampled {0} files in {1} sets'\
            .format(sum(map(len, names)), len(names), label)
    with tde.util.verb_print('  token/type ({0}): calculating scores'
                             .format(label), verbose, False, True, False):
        pto, rto, pty, rty = izip(*(et(clsdict.restrict(ns, False),
                                       wrd_corpus)
                                    for ns in names))
    pto = np.fromiter(pto, dtype=np.double)
    rto = np.fromiter(rto, dtype=np.double)
    pty = np.fromiter(pty, dtype=np.double)
    rty = np.fromiter(rty, dtype=np.double)
    return pto, rto, pty, rty

def token_type(disc_clsdict, wrd_corpus, names_within, names_cross,
               dest, verbose, n_jobs):
    if verbose:
        print tde.util.dbg_banner('TOKEN/TYPE')
    ptoc, rtoc, ptyc, rtyc = _token_type_sub(disc_clsdict, wrd_corpus,
                                             names_cross, 'cross',
                                             verbose, n_jobs)
    ftoc = np.vectorize(tde.util.fscore)(ptoc, rtoc)
    ftyc = np.vectorize(tde.util.fscore)(ptyc, rtyc)

    ptow, rtow, ptyw, rtyw = _token_type_sub(disc_clsdict, wrd_corpus,
                                             names_within, 'within',
                                             verbose, n_jobs)
    ftow = np.vectorize(tde.util.fscore)(ptow, rtow)
    ftyw = np.vectorize(tde.util.fscore)(ptyw, rtyw)
    with open(path.join(dest, 'token_type'), 'w') as fid:
        fid.write(tde.util.pretty_scores(ptoc, rtoc, ftoc, 'token cross-speaker',
                                         len(names_cross),
                                         sum(map(len, names_cross))))
        fid.write('\n')
        fid.write(tde.util.pretty_scores(ptyc, rtyc, ftyc, 'type cross-speaker',
                                         len(names_cross),
                                         sum(map(len, names_cross))))
        fid.write('\n')
        fid.write(tde.util.pretty_scores(ptow, rtow, ftow, 'token within-speaker',
                                         len(names_within),
                                         sum(map(len, names_within))))
        fid.write('\n')
        fid.write(tde.util.pretty_scores(ptyw, rtyw, ftyw, 'type within-speaker',
                                         len(names_within),
                                         sum(map(len, names_within))))

def _nlp_sub(disc_clsdict, gold_clsdict, names, label, verbose, n_jobs):
    # ned
    ned = tde.nlp.NED
    cov = tde.nlp.coverage
    if verbose:
        print '  token/type ({2}): subsampled {0} files in {1} sets'\
            .format(sum(map(len, names)), len(names), label)
    with tde.util.verb_print('  nlp ({0}): calculating scores'
                             .format(label), verbose, False, True, False):
        ned_score = Parallel(n_jobs=n_jobs,
                             verbose=5 if verbose else 0,
                             pre_dispatch='n_jobs')(delayed(ned)\
                                                    (disc_clsdict.restrict(ns,
                                                                           True))
                                                    for ns in names)
        cov_score = Parallel(n_jobs=n_jobs,
                             verbose=5 if verbose else 0,
                             pre_dispatch='n_jobs')(delayed(cov)\
                                                    (disc_clsdict.restrict(ns,
                                                                           False),
                                                     gold_clsdict.restrict(ns,
                                                                           False))
                                    for ns in names)
    return np.array(ned_score), np.array(cov_score)


def nlp(disc_clsdict, gold_clsdict, names_within, names_cross,
        dest, verbose, n_jobs):
    if verbose:
        print tde.util.dbg_banner('NLP')
    nc, cc = _nlp_sub(disc_clsdict, gold_clsdict, names_cross, 'cross',
                      verbose, n_jobs)
    nw, cw = _nlp_sub(disc_clsdict, gold_clsdict, names_within, 'within',
                      verbose, n_jobs)
    with open(path.join(dest, 'nlp'), 'w') as fid:
        fid.write(tde.nlp.pretty_score(nc, cc, 'NLP within-speaker',
                                       len(names_within),
                                       sum(map(len, names_within))))
        fid.write('\n')
        fid.write(tde.nlp.pretty_score(nw, cw, 'NLP cross-speaker',
                                       len(names_cross),
                                       sum(map(len, names_cross))))


def _boundary_sub(disc_clsdict, corpus, names, label, verbose, n_jobs):
    eb = tde.boundaries.eval_from_bounds
    if verbose:
        print '  boundary ({2}): subsampled {0} files in {1} sets'\
            .format(sum(map(len, names)), len(names), label)
    with tde.util.verb_print('  boundary ({0}): calculating scores'
                             .format(label), verbose, True, True, True):
        disc_bounds = [tde.boundaries.Boundaries(disc_clsdict.restrict(ns))
                       for ns in names]
        gold_bounds = [tde.boundaries.Boundaries(corpus.restrict(ns))
                       for ns in names]
    with tde.util.verb_print('  boundary ({0}): calculating scores'
                             .format(label), verbose, False, True, False):
        p, r = izip(*Parallel(n_jobs=n_jobs, verbose=5 if verbose else 0,
                              pre_dispatch='2*n_jobs') \
                    (delayed(eb)(disc, gold)
                     for disc, gold in zip(disc_bounds, gold_bounds)))
    return np.fromiter(p, dtype=np.double), np.fromiter(r, dtype=np.double)


def boundary(disc_clsdict, corpus, names_within, names_cross,
               dest, verbose, n_jobs):
    if verbose:
        print tde.util.dbg_banner('BOUNDARY')
    pc, rc = _boundary_sub(disc_clsdict, corpus, names_cross,
                           'cross', verbose, n_jobs)
    fc = np.vectorize(tde.util.fscore)(pc, rc)
    pw, rw = _boundary_sub(disc_clsdict, corpus, names_within,
                           'within', verbose, n_jobs)
    fw = np.vectorize(tde.util.fscore)(pw, rw)
    with open(path.join(dest, 'boundary'), 'w') as fid:
        fid.write(tde.util.pretty_scores(pc, rc, fc, 'boundary cross-speaker',
                                         len(names_cross),
                                         sum(map(len, names_cross))))
        fid.write('\n')
        fid.write(tde.util.pretty_scores(pw, rw, fw, 'boundary within-speaker',
                                         len(names_within),
                                         sum(map(len, names_within))))

def _load_corpus(fname):
    return tde.reader.load_corpus_txt(fname)

def load_wrd_corpus(wrd_corpus_file, verbose):
    with tde.util.verb_print('  loading word corpus file',
                             verbose, True, True, True):
        wrd_corpus = _load_corpus(wrd_corpus_file)
    return wrd_corpus

def load_phn_corpus(phn_corpus_file, verbose):
    with tde.util.verb_print('  loading phone corpus file',
                             verbose, True, True, True):
        phn_corpus = _load_corpus(phn_corpus_file)
    return phn_corpus

def _load_names(fname):
    names = [[]]
    for line in open(fname):
        if line == '\n':
            names.append([])
        else:
            names[-1].append(line.strip())
    return names

def load_names_cross(fname, verbose):
    with tde.util.verb_print('  loading folds cross',
                             verbose, True, True, True):
        names = _load_names(fname)
    return names

def load_names_within(fname, verbose):
    with tde.util.verb_print('  loading folds within',
                             verbose, True, True, True):
        names = _load_names(fname)
    return names

def _load_classes(fname, corpus):
    return tde.reader.load_classes_txt(fname, corpus)

def load_disc(fname, corpus, verbose):
    with tde.util.verb_print('  loading discovered classes',
                             verbose, True, True, True):
        disc = _load_classes(fname, corpus)
    return disc

def load_gold(fname, corpus, verbose):
    with tde.util.verb_print('  loading gold classes',
                             verbose, True, True, True):
        gold = _load_classes(fname, corpus)
    return gold

if __name__ == '__main__':
    import argparse
    def parse_args():
        parser = argparse.ArgumentParser(
            prog='eval2',
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description='Evaluate spoken term discovery',
            epilog="""Example usage:

$ ./eval_track2.py sample my_sample.classes resultsdir/

evaluates STD output `my_sample.classes` on the sample dataset and stores the
output in `resultsdir/`.

$ ./eval2 english my_english.classes resultsdir/

evaluates STD performance on the english dataset.

Classfiles must be formatted like this:

Class 1 (optional_name)
fileID starttime endtime
fileID starttime endtime
...

Class 2 (optional_name)
fileID starttime endtime
...
""")
        parser.add_argument('dataset_id',
                            choices=['french', 'english', 'mandarin'],
                            help='identifier for the dataset used')
        parser.add_argument('disc_clsfile', metavar='DISCCLSFILE',
                            nargs=1,
                            help='discovered classes')
        parser.add_argument('outdir', metavar='DESTINATION',
                            nargs=1,
                            help='location for the evaluation results')
        parser.add_argument('-v', '--verbose',
                            action='store_true',
                            dest='verbose',
                            default=False,
                            help='display progress')
        parser.add_argument('-j', '--n-jobs',
                            action='store',
                            type=int,
                            dest='n_jobs',
                            default=1,
                            help='number of cores to use')
        parser.add_argument('-V', '--version', action='version',
                            version="%(prog)s version {version}".format(version=VERSION))
        return vars(parser.parse_args())
    args = parse_args()

    verbose = args['verbose']
    n_jobs = args['n_jobs']

    dataset_id = args['dataset_id']
    disc_clsfile = args['disc_clsfile'][0]
    dest = args['outdir'][0]

    valid_ids = ['french', 'english', 'mandarin']

    if getattr(sys, 'frozen', False):
        # frozen
        rdir = path.dirname(sys.executable)
        resource_dir = path.join(rdir, 'resources', 'resources')
    else:
        # unfrozen
        rdir = path.dirname(path.realpath(__file__))
        resource_dir = path.join(rdir, 'resources')


    if dataset_id == 'french':
        names_cross_file  = path.join(resource_dir, 'french.names.cross')
        names_within_file = path.join(resource_dir, 'french.names.within')
        gold_clsfile      = path.join(resource_dir, 'french.classes')
        phn_corpus_file   = path.join(resource_dir, 'french.phn')
        wrd_corpus_file   = path.join(resource_dir, 'french.wrd')
    elif dataset_id == 'english':
        names_cross_file  = path.join(resource_dir, 'english.names.cross')
        names_within_file = path.join(resource_dir, 'english.names.within')
        gold_clsfile      = path.join(resource_dir, 'english.classes')
        phn_corpus_file   = path.join(resource_dir, 'english.phn')
        wrd_corpus_file   = path.join(resource_dir, 'english.wrd')
    elif dataset_id == 'mandarin':
        names_cross_file  = path.join(resource_dir, 'mandarin.names.cross')
        names_within_file = path.join(resource_dir, 'mandarin.names.within')
        gold_clsfile      = path.join(resource_dir, 'mandarin.classes')
        phn_corpus_file   = path.join(resource_dir, 'mandarin.phn')
        wrd_corpus_file   = path.join(resource_dir, 'mandarin.wrd')
    else:
        print 'unknown datasetID: {0}. datasetID must be one of [{1}]'.format(
            dataset_id, ','.join(valid_ids))
        exit()

    if verbose:
        print 'eval2 version {0}'.format(VERSION)
        print '-------------------'
        print 'dataset:     {0}'.format(dataset_id)
        print 'inputfile:   {0}'.format(disc_clsfile)
        print 'destination: {0}'.format(dest)
        print

    if verbose:
        print tde.util.dbg_banner('LOADING FILES')

    wrd_corpus = load_wrd_corpus(wrd_corpus_file, verbose)
    phn_corpus = load_phn_corpus(phn_corpus_file, verbose)

    names_cross = load_names_cross(names_cross_file, verbose)
    names_within = load_names_within(names_within_file, verbose)

    disc_clsdict = load_disc(disc_clsfile, phn_corpus, verbose)
    gold_clsdict = load_gold(gold_clsfile, phn_corpus, verbose)

    try:
        os.makedirs(dest)
    except OSError:
        pass

    with open(path.join(dest, 'VERSION_{0}'.format(VERSION)), 'w') as fid:
        fid.write('')

    match(disc_clsdict, gold_clsdict, phn_corpus, names_within, names_cross,
          dest, verbose, n_jobs)
    group(disc_clsdict, names_within, names_cross, dest, verbose, n_jobs)
    token_type(disc_clsdict, wrd_corpus, names_within, names_cross,
               dest, verbose, n_jobs)
    nlp(disc_clsdict, gold_clsdict, names_within, names_cross,
        dest, verbose, n_jobs)
    boundary(disc_clsdict, wrd_corpus, names_within, names_cross, dest,
             verbose, n_jobs)
