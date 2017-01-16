#!/usr/bin/python

#
# Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
#


from __future__ import division

import sys
import os
import re

from optparse import OptionParser

parser = OptionParser()

parser.add_option("--input", help="edge list input file", dest="edgelist")
parser.add_option("--output", help="Output filename", dest="outfile")
parser.add_option("--thresh", help="Min edge weight", default="0", dest="thresh")
parser.add_option("--numnodes", help="number of nodes",dest="numnodes")

(options, args) = parser.parse_args() 

inFilename = options.edgelist
outFilename = options.outfile
thresh = int(options.thresh)
numnodes = int(options.numnodes)

# Merge Groups #
def colorcomp(neighbors,color,root,ncomp):
    trail = []
    trail.extend(list(neighbors[root]))

    while trail:
        v = trail.pop()
        if color[v] == 0:
            color[v] = ncomp
            trail.extend(list(neighbors[v]))
    
    return color


print "Creating data structures"

color = [0]*numnodes
neighbors = [list() for index in xrange(numnodes)]
c=0
for line in open(inFilename,"r"):
    (A,B,val) = line.strip().split("\t") 
    A = int(A)
    B = int(B)
    val = int(val)
    if val > thresh:
        if len(neighbors[A-1]) < 10000:
            neighbors[A-1].append(B-1)
        if len(neighbors[B-1]) < 10000:
            neighbors[B-1].append(A-1)

    c += 1
    if c % 1000000 == 0: 
        print "Finished reading",c,"edges"

print "Running DFS"
    
ncomp = 0
for v in range(numnodes):
    if color[v] > 0: 
        continue

    ncomp = ncomp + 1
    color[v] = ncomp
    color = colorcomp(neighbors,color,v,ncomp)


print "Writing to file:",outFilename

outpairs = []
for n in range(len(color)):
    outpairs.append((n,color[n]))
outpairs.sort(key = lambda x: x[1])

OUT = open(outFilename,'w')
lastcol = outpairs[0][1]
for p in outpairs:
    if p[1] != lastcol:
        OUT.write( "\n" )
    OUT.write( str(p[0]+1)+" " )
    lastcol = p[1]
OUT.write( "\n" )
OUT.close()
            
            

            








