#!/usr/bin/python

#
# Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
#


from __future__ import division

import sys
import os
import re
import string
import math

from optparse import OptionParser

parser = OptionParser()

parser.add_option("--cinput", help="clusters input file", dest="cinfile")
parser.add_option("--ninput", help="node input file", dest="ninfile")
parser.add_option("--listname", help="filelist name", dest="listname")
parser.add_option("--outdir", help="output lattice director", dest="outdir")
parser.add_option("--x0", help="sigmoid center DTW score", dest="x0")

(options, args) = parser.parse_args() 

cinFilename = options.cinfile
ninFilename = options.ninfile
listname = options.listname
outdir = options.outdir
x0 = float(options.x0)

print "Reading the file list"
docs = []
docarcs = {}
for line in open(listname):
    f = line.strip() 
    docs.append((f))
    docarcs[f] = []

print "Reading the node set"
nodes = []
for line in open(ninFilename):
    (f,xA,xB,prob,rho,fid) = line.strip().split()
    nodes.append((f,int(xA),int(xB),int(fid),float(prob)))

print "Processing clusters"
clid = 0
for line in open(cinFilename):
    clid = clid + 1
    cluster = line.strip().split()
    
    for c in cluster:
        node = nodes[int(c)-1]
        muname = node[0]
        docarcs[muname].append((node[1],node[2],clid,node[4]))

print "Writing lattices to files"
for f in docs:
    arcs = docarcs[f]

    # Collect node times and create map from node times to node ids and v.v.
    nodetimes = sorted(list(set( [ x[0] for x in arcs ] + [ x[1] for x in arcs ] )))
    if len(nodetimes) == 0 or nodetimes[0] != 0:
        nodetimes.insert(0,0.0)
        nodetimes.insert(0,0.0)

    t2n = {}
    n2t = {}
    for n in range(len(nodetimes)):
        t2n[nodetimes[n]] = n+1
        n2t[n+1] = nodetimes[n]
    
    # Generate output arc set and sort
    outarcs = []
    for n in range(len(nodetimes)-1):
        outarcs.append((n+1,n+2,0,0.00001))

    alpha = 100
    for a in arcs:
        score = 1/(1+math.exp(-alpha*(a[3]-x0)))
        outarcs.append((t2n[a[0]],t2n[a[1]],a[2],score))

    # Sort the arcs by start time
    outarcs.sort(key = lambda x: x[0])

    base = os.path.splitext(os.path.basename(f))[0]
    outfile = outdir + "/" + base + ".slf"
    dout = open(outfile,"w")

    # Write header    
    dout.write("VERSION=1.1\n")
    dout.write("UTTERANCE=" + str(base) + "\n")
    dout.write("base=0.0\n")
    dout.write("N=" + str(len(nodetimes)) + "\tL=" + str(len(outarcs)) + "\n")

    # Write the nodes
    for n in range(len(nodetimes)):
        dout.write("I="+str(n) + "\tt=" + str(float(n2t[n+1])/100) + "\n")
                   
    # Write the arcs
    cnt=0
    for a in outarcs:
        dout.write("J="+str(cnt)+"\tS="+str(a[0]-1) + "\tE="+str(a[1]-1) + \
                       "\tW=PT"+str(a[2]) + "\ta="+str(round(a[3],7)) + \
                       "\tl=1.0\n")
        cnt = cnt + 1
    
    dout.close()

