#
# Copyright 2011-2012  Johns Hopkins University (Author: Greg Sell)
#


import sys, os, getopt
import numpy as np
import wave, struct, math
import scipy.signal as dsp
import sklearn.mixture as mix

def markSpeechInListToDir(audiolist,outdir,p) :
    fAudio = open(audiolist,'r');
    for audiofile in fAudio :
        audiofile = audiofile.strip();
        outfile = outdir+"/"+audiofile.split('/')[-1][:-3]+"csm";
        if p['verbose'] : print("Marking "+audiofile+" to "+outfile+"...");
        markSpeechInFile(audiofile,outfile,p);

def markSpeechInListToList(audiolist,outlist,p) :
    fAudio = open(audiolist,'r');
    fOut = open(outlist,'r');
    for audiofile in fAudio :
        audiofile = audiofile.strip();
        outfile = fOut.readline();
        if p['verbose'] : print("Marking "+audiofile+" to "+outfile+"...");
        markSpeechInFile(audiofile,outfile,p);

    fAudio.close();
    fOut.close();

def markSpeechInFile(audiofile,outfile,p) :
    e_min = p['energy_min'];
    e = getEnergyForFile(audiofile,p);
    if e_min==0 :
        mark = adaptiveThresh(e);
    else :
        mark = np.zeros(len(e));
        for n in range(len(e)) :
            if(e[n] > e_min) :
                mark[n] = 1;

    fr = p['framerate'];
    sm = p['speech_min'];
    qm = p['quiet_min'];
    mark_str = ""; 
    state = 0;
    start = 10000000;
    end = 0;
    for n in range(len(mark)) :
        if mark[n] != state :
            state = mark[n];
            if state == 1 :
                if (float(n)/fr-end >= qm) or start==10000000 : #(float(n)/fr <= qm) :
                    start = float(n)/fr;
                else :
                    mark_str = "\n".join(mark_str.split("\n")[:-2]);
                    if(len(mark_str)>0) :
                        mark_str += "\n"
            
            elif state == 0 :
                if(float(n)/fr-start >= sm) :
                    end = float(n)/fr;
                    mark_str += "%.2f %.2f\n"%(start,end);

    if state == 1 and float(len(mark))/fr-start >= sm : mark_str += "%.2f %.2f\n"%(start,float(len(mark))/fr);
    f = open(outfile,'w');
    f.write(mark_str);
    f.close();

def getEnergyForFile(audiofile,p) :
    
    [y,fs] = openAudio(audiofile);

    if(fs!=p['fs'] and p['fs'] != 0) :
        y = dsp.resample(y,int(y.shape[0]*p['fs']/fs+1));
        fs = p['fs']

    N = y.shape[0];
    if p['verbose'] : print "File duration = %.2f seconds"%(float(len(y))/fs);

    y += p['dither']*np.random.randn(len(y),1);

    fr = p['framerate'];
    framelen = 2*int(round(fs/fr));
    framehop = int(round(fs/fr));

    e = np.ones(int(np.ceil(float(N)/fs*fr)),dtype=float);
    for n in range(len(e)) : 
        frame = y[(framehop*n):(framehop*n+framelen)];
        e[n] = ((frame**2).sum()/len(frame))**0.5;
    
    return e

def adaptiveThresh(e) :
    gmm = mix.GMM(n_components=3);
    e = np.log(e);
    e = np.array(e,ndmin=2).transpose()
    gmm.fit(e)
    labels = gmm.predict(e);
    marks = np.zeros(len(e));
    sp_ind = gmm.means_.argmax();
    marks[labels==sp_ind] = 1;
    return marks;

def openAudio(fn) :
    if fn[-3:] == 'wav' :
        wf = wave.open(fn,"rb");
        fs = wf.getframerate();
        N = wf.getnframes();
        C = wf.getnchannels();
        nbytes = wf.getsampwidth();
        bi = wf.readframes(N);
        readlen = len(bi)/nbytes;
        x = (np.array(struct.unpack("%dh"%(readlen),bi),dtype=np.float32).reshape((readlen/C,C)))/(2**15-1);
        wf.close();
    else :
        print 'Invalid audio file extension "%s" - only wav is accepted'%(audiofile[-3:]);
        sys.exit(2);

    if x.shape[1] > 1 :
        if p['channel'] == -1 :
            np.sum(x,axis=1);
        else :
            x = x[:,p['channel']];

    return [x,fs]

def usage() :
    print "\nmark_energy : Marks for a wav file based on RMS energy"
    print "\tMarks regions of high energy in given .wav file or list of" 
    print "\t.wav files.  Output is written to file as speech boundary"
    print "\tmarks in seconds.  Each segment is given its own line.\n"
    print "\tUSAGE:"
    print "\t>> python mark_energy.py [opts]"
    print "\t   -h Print usage"
    print "\t   -i Input .wav file or list of .wav files (REQUIRED)"
    print "\t   -o Output directory or list of file names"
    print "\t   -f Frames per second (default = 20)"
    print "\t   -c Selects 0-indexed channel for multi-channel audio, -1 sums (default = -1)"
    print "\t   -e Minimum RMS energy. Set to 0 for GMM adaptive thresholding (default = 0.01)"
    print "\t   -d Energy of dithered noise (default = 0.001)"
    print "\t   -s Minimum speech duration (default = 0.0 seconds)"
    print "\t   -q Minimum quiet duration (default = 0.0 seconds)"
    print "\n\tIf an output directory is given, output files are"
    print "named after the corresponding input .wav file with"
    print "a .nrg extension instead of .wav\n"

def main(argv) :
    try :
        [opts, args] = getopt.getopt(argv,'c:d:f:hi:o:e:s:q:v');
    except getopt.GetoptError as err:
        print str(err)
        usage();
        sys.exit(1);

    # Defaults
    p = {};
    infile = None;
    outfile = None;
    p['framerate'] = 20;
    p['energy_min'] = 0.01;
    p['quiet_min'] = 0.0;
    p['speech_min'] = 0.0;
    p['fs'] = 0;
    p['dither'] = 0.001;
    p['verbose'] = False;
    p['channel'] = -1;
    
    for opt, arg in opts :
        if opt=='-i' :
            infile = arg;
        elif opt=='-o' :
            outfile = arg;
        elif opt=='-f' :
            p['framerate'] = int(arg);
        elif opt=="-q" :
            p['quiet_min'] = float(arg);
        elif opt=="-s" :
            p['speech_min'] = float(arg);
        elif opt=="-e" :
            p['energy_min'] = float(arg);
        elif opt=="-d" :
            p['dither'] = float(arg);
        elif opt=="-v" :
            p['verbose'] = True;
        elif opt=="-c" :
            p['channel'] = int(arg);
        elif opt=='-h' :
            usage();
            sys.exit(0);
    
    if infile is None :
        print "Error: No input file given"
        usage()
    
    if os.path.isfile(infile) :
        if infile[-3:] == 'wav' :
            if os.path.isfile(outfile) :
                # infile is file, outfile already exists so is assumed to be file to be overwritten
                markSpeechInFile(infile,outfile,p);
            elif os.path.isdir(outfile) :
                # infile is file, outfile is a directory
                outfile = outfile+"/"+infile.split('/')[-1][:-3]+"csm";
                markSpeechInFile(infile,outfile,p);
            else :
                # infile is file, outfile does not exist so is assumed to be filename
                markSpeechInFile(infile,outfile,p);
        else :
            if os.path.isfile(outfile) :
                # infile is list, outfile already exists so is assumed to be a list
                markSpeechInListToList(infile,outfile,p);
            elif os.path.isdir(outfile) :
                # infile is list, oufile is directory
                markSpeechInListToDir(infile,outfile,p);
            else :
                # infile is list, outfile does not exist so is assumed to be directory
                os.system("mkdir "+outfile);
                markSpeechInListToDir(infile,outfile,p);
    else :
        print("ERROR : Input file or list not found (%s)"%infile);
        sys.exit(2);

if __name__ == '__main__' :
    main(sys.argv[1:]);
