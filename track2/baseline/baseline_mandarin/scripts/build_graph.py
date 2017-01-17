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

parser.add_option("--input", help="matches input file", dest="infile")
parser.add_option("--probthr", help="match probability threshold", dest="probthr", default="0.5")
parser.add_option("--olapthr", help="overlap threshold", dest="olapthr", default="0.95")
parser.add_option("--output", help="graph output base", dest="outfile")
parser.add_option("--list", help="file basename list", dest="filelist", default="")

(options, args) = parser.parse_args() 

inFilename = options.infile
outFilebase = options.outfile
probthr = float(options.probthr)
olapthr = float(options.olapthr)
filelist = options.filelist

vout = open(outFilebase + ".nodes","w")
eout = open(outFilebase + ".edges","w")

print "Building the file set"
if len(filelist) == 0:
    fileset = set()
    for line in open(inFilename):
        if len(line.strip().split()) == 2:
            [f1,f2] = line.strip().split()
            fileset.add(f1);
            fileset.add(f2);    

    fileset = list(fileset)
    fileset.sort()
else:
    fileset = []
    for line in open(filelist):
        f1 = line.strip().split()
        fileset.append(f1[0]);
    fileset.sort()

fileind = {}
for f in fileset:
    ind = fileset.index(f)
    fileind[f] = ind+1

vcount = 0;
ecount = 0;

print "Generating Nodes and Type 1 Edges"
savefAB = []
cnt = 0
for line in open(inFilename):
    if len(line.strip().split()) == 2:
        [f1,f2] = line.strip().split()
        continue

    [xA,xB,yA,yB,prob,rho] = line.strip().split()
    if float(prob) < probthr:
        continue

#    if float(prob) > 0.95:
#        continue

    i1 = fileind[f1]
    i2 = fileind[f2]

    if int(xB) > int(xA):
        vcount = vcount + 1
        vout.write(f1+"\t"+xA+"\t"+xB+"\t"+prob+"\t"+rho+"\t"+str(i1)+"\n")
        savefAB.append((vcount,f1,int(xA),int(xB)))
    
    if int(yB) > int(yA):
        vcount = vcount + 1
        vout.write(f2+"\t"+yA+"\t"+yB+"\t"+prob+"\t"+rho+"\t"+str(i2)+"\n")
        savefAB.append((vcount,f2,int(yA),int(yB)))

    ecount = ecount + 1
    eout.write(str(vcount-1)+"\t"+str(vcount)+"\t"+str(int(float(prob)*1000))+"\n")

    cnt += 1
    if cnt % 100000 == 0:
        print "Finished ingesting",cnt
    
print "Original Nodes: ", vcount
print "Original Edges: ", ecount

# Add overlap edges
print "Adding Overlap Edges"

savefAB.sort(key=lambda x: (x[1],x[2]))

pos1 = 1
while pos1 < len(savefAB):
    pos2 = pos1 + 1
    
    while pos2 < len(savefAB) and savefAB[pos2][1] == savefAB[pos1][1]:
        pos2 = pos2 + 1

    print "Processing",str(pos1)+":"+str(pos2),"of",len(savefAB)
    for n in range(pos1,pos2-1):
        for m in range(n+1,pos2-1):
            if savefAB[m][2] > savefAB[n][3]:
                break
            num = savefAB[n][3]-savefAB[n][2] + savefAB[m][3]-savefAB[m][2]
            den = max(savefAB[n][3],savefAB[m][3]) - min(savefAB[n][2],savefAB[m][2])
            olap = max(0,num/den-1)
            
            if olap >= olapthr:
                ecount = ecount + 1
                eout.write(str(savefAB[n][0])+"\t"+str(savefAB[m][0])+"\t"+str(int(float(olap)*1000))+"\n") 
                #print savefAB[n], savefAB[m],olap

    pos1 = pos2
  
print "Total Edges: ", ecount

vout.close()
eout.close()
