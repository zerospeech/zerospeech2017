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

def multi_delete( L, inds ):
    inds = sorted(inds,reverse=True)
    for i in inds:
        del L[i]
    return L

parser = OptionParser()

parser.add_option("--cinput", help="clusters input file", dest="cinfile")
parser.add_option("--ninput", help="nodes input file", dest="ninfile")
parser.add_option("--dedupthr", help="overlap threshold for dup", dest="dedupthr", default="0.9")
parser.add_option("--output", help="dedup'd cluster file", dest="outfile")

(options, args) = parser.parse_args() 

cinFilename = options.cinfile
ninFilename = options.ninfile
outFilename = options.outfile
dedupthr = float(options.dedupthr)

print "Reading the node set"
nodes = []
for line in open(ninFilename):
    (f,xA,xB,prob,rho,fid) = line.strip().split()
    #print f,xA,xB,prob,rho,fid
    nodes.append((int(xA),int(xB),int(fid)))

print "Processing clusters"

dout = open(outFilename,"w")

count = 0
for line in open(cinFilename):
    cluster = line.strip().split()
    #print "-----------------------"
    #print cluster

    clnodes = [(nodes[int(c)-1][0],nodes[int(c)-1][1],nodes[int(c)-1][2],c) for c in cluster]
    clnodes.sort(key=lambda x: (x[2],x[1]))

    #print clnodes
    A = 0
    B = A
    to_remove = []
    while A < len(clnodes):
        while B < len(clnodes) and clnodes[A][2] == clnodes[B][2]:
            B = B + 1
        
        #print A, B, len(clnodes)

        to_remove_file = []
        for n in range(A,B):
            if n in to_remove_file:
                continue
            for m in range(n+1,B):
                if m in to_remove_file:
                    continue

                xA = clnodes[n][0]
                xB = clnodes[n][1]
                yA = clnodes[m][0]
                yB = clnodes[m][1]
                num = yB-yA + xB-xA
                den = max(xB,yB) - min(xA,yA)
                folap = max(0,num/den-1)

                if folap >= dedupthr:
                    to_remove_file.append(m)

        to_remove.extend(to_remove_file)
        A = B

    clnodes = multi_delete(clnodes, to_remove)
    cluster = [c[3] for c in clnodes]

    dout.write(cluster[0])
    for c in cluster[1:]:
        dout.write(" "+c)
    dout.write("\n")

    count = count + 1
    if count % 10000 == 0:
        print "Finished ",count

dout.close()

