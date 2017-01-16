#!/usr/bin/python

#
# Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
#


from __future__ import division

import sys
import os
import re
import string

from optparse import OptionParser

parser = OptionParser()

parser.add_option("--cinput", help="clusters input file", dest="cinfile")
parser.add_option("--ninput", help="nodes input file", dest="ninfile")
parser.add_option("--maxfreq", help="maximum corpus frequency", dest="maxfreq", default="10000000")
parser.add_option("--minfreq", help="minimum corpus frequency", dest="minfreq", default="1")
parser.add_option("--output", help="dedup'd cluster file", dest="outfile")

(options, args) = parser.parse_args() 

cinFilename = options.cinfile
ninFilename = options.ninfile
outFilename = options.outfile

maxfreq = int(options.maxfreq)
minfreq = int(options.minfreq)

print "Reading the node set"
nodes = []
for line in open(ninFilename):
    (f,xA,xB,prob,rho,fid) = line.strip().split()
    #print f,xA,xB,prob,rho,fid
    spkr = f[0:3]
    nodestr = '%s %s %d %d' % (f,spkr,int(xA),int(xB))
    nodes.append(nodestr)

print "Processing clusters"

dout = open(outFilename,"w")

count = 0
for line in open(cinFilename):
    count = count + 1
    cluster = line.strip().split()
    if len(cluster) <= maxfreq and len(cluster) >= minfreq:
        for n in range(len(cluster)):
            for m in range(n+1,len(cluster)):
                dout.write('PT%d %s %s\n' % (count,nodes[int(cluster[n])-1],nodes[int(cluster[m])-1]))

    if count % 10000 == 0:
        print "Finished ",count

dout.close()

