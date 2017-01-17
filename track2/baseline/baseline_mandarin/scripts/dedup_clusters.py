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

    if len(cluster) < 10000:
        n = 0
        while n < len(cluster):
            to_remove = []
            i1 = int(cluster[n])-1
            for m in range(n+1,len(cluster)):
                i2 = int(cluster[m])-1
                if nodes[i1][2] == nodes[i2][2]:
                    xA = nodes[i1][0]
                    xB = nodes[i1][1]
                    yA = nodes[i2][0]
                    yB = nodes[i2][1]
                    num = yB-yA + xB-xA
                    den = max(xB,yB) - min(xA,yA)
                    folap = max(0,num/den-1)
                
                    if folap >= dedupthr:
                        to_remove.append(m)

            to_remove.reverse()
            for m in to_remove:
                cluster.remove(cluster[m])
        #print cluster
            n = n + 1

    dout.write(cluster[0])
    for c in cluster[1:]:
        dout.write(" "+c)
    dout.write("\n")

    count = count + 1
    if count % 10000 == 0:
        print "Finished ",count

dout.close()

