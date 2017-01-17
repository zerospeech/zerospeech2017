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
parser.add_option("--ninput", help="node input file", dest="ninfile")
parser.add_option("--listname", help="filelist name", dest="listname")
parser.add_option("--output", help="output feature file", dest="outfile")

(options, args) = parser.parse_args() 

cinFilename = options.cinfile
ninFilename = options.ninfile
listname = options.listname
outfile = options.outfile

print "Reading the file list"
docs = []
t_by_doc = {}
for line in open(listname):
    f = line.strip() 
    docs.append((f))
    t_by_doc[f] = []

print "Reading the node set"
nodes = []
for line in open(ninFilename):
    (f,xA,xB,prob,rho,fid) = line.strip().split()
    nodes.append((f,int(xA),int(xB),int(fid),prob))

print "Processing clusters"
clid = 0
for line in open(cinFilename):
    clid = clid + 1
    cluster = line.strip().split()
    
    for c in cluster:
        node = nodes[int(c)-1]
        muname = node[0]
        t_by_doc[muname].append(clid)

print "Writing to file"
dout = open(outfile,"w")
for f in docs:
    t_by_doc[f] = sorted(t_by_doc[f])
    spcnt = {}
    for c in t_by_doc[f]:
        if spcnt.has_key(c):
            spcnt[c] = spcnt[c] + 1
        else:
            spcnt[c] = 1

    outline = f + " "
    for p in sorted(spcnt.keys()):
        outline = outline + str(p) + ":" + str(spcnt[p]) + " "
    
    dout.write(outline + "\n")

dout.close()

