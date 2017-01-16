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

(options, args) = parser.parse_args() 

inFilename = options.edgelist
outFilename = options.outfile
thresh = int(options.thresh)


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


print "Reading edges from", inFilename

IN = open(inFilename)
lines = [line.strip().split("\t") for line in IN.readlines()]
edges = [(int(x[0])-1,int(x[1])-1) for x in lines if int(x[2]) >= thresh]
numnodes = max([max(x[0],x[1]) for x in edges])+1
IN.close()

print "Node count:",str(numnodes)
print "Edge count:",str(len(edges))


print "Creating data structures"

color = [0]*numnodes
neighbors = [set() for index in xrange(numnodes)]
for e in edges:
    neighbors[e[0]].add(e[1])
    neighbors[e[1]].add(e[0])


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
            
            

            








