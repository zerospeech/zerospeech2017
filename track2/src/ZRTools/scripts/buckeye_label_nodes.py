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


def olap( A, B ):
    num = A[1]-A[0] + B[1]-B[0]
    den = max(A[1],B[1])-min(A[0],B[0])
    return max(0,num/den-1)

parser = OptionParser()

parser.add_option("--ninput", help="nodes input file", dest="ninfile")
parser.add_option("--output", help="label file", dest="outfile")

(options, args) = parser.parse_args() 

ninFilename = options.ninfile
outFilename = options.outfile

print "Reading the node set"
nodes = []
lineno = 0
for line in open(ninFilename):
    (f,xA,xB,prob,rho,fid) = line.strip().split()
    #print f,xA,xB,prob,rho,fid
    lineno += 1
    nodes.append((int(xA),int(xB),f,lineno))

nodes.sort( key=lambda x: (x[2],x[1]) )

print "Processing nodes"
dout = open(outFilename,"w")
count = 0
lastside=""
ref = []

outlabels = []
for x in nodes:
    if x[2] != lastside:
        lastside = x[2]
        trdir = '/home/hltcoe/ajansen/BUCKEYE/'
        trfile = trdir + x[2][:3] + '/' + x[2] + '.words'

        ref = []
        foundhash = False
        tA = 0
        for t in open(trfile,'r'):
            if t[0] == '#':
                foundhash = True
                continue

            if not foundhash:
                continue

            if '<' in t or '{' in t:
                (tB) = t.strip().split(' ')[0]
                tA = tB
                continue

            (tB,ign,tok) = t.strip().split(';')[0].split()
            ref.append( (int(float(tA)*100),int(float(tB)*100),tok) )
            tA = tB
            
    tlist = [t for t in ref if olap(x,t)>0]
    tlist.sort(key=lambda x: x[0])

    label=""
    for t in tlist:        
        left = max(0,float(x[0]-t[0])/(t[1]-t[0]))
        right = min(1,float(x[1]-t[0])/(t[1]-t[0]))        

        if right-left < 0.66:
            continue

        #left = int(math.floor(left*len(t[2])))
        #right = int(math.ceil(right*len(t[2])))
        #if len(t[2]) > 1 and right-left == 1:
        #    continue
        #ss = t[2][left:right]

        ss = t[2]
        label = label + ss + " "

    label.strip()
    if len(label) == 0:
        label = tlist[0][2]

    outlabels.append((x[3],label))

    count = count + 1
    if count % 1000 == 0:
        print "Finished ",count

outlabels.sort()
for label in outlabels:
    dout.write(label[1]+'\n')

dout.close()

