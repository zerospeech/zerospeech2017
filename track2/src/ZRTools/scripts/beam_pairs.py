#!/usr/bin/env python

#
# Copyright 2011-2012  Johns Hopkins University (Author: Aren Jansen)
#

from __future__ import division

import sys
import os
import re
import string
import random

beamwidth = int(sys.argv[1])

baselist = []
for line in sys.stdin:
    base = line.strip()
    baselist.append(base)

if beamwidth == 0:
    random.shuffle(baselist)
    for n in range(len(baselist)):
        for m in range(n,len(baselist)):
            sys.stdout.write(baselist[n]+" "+baselist[m]+"\n")
else:
    for n in range(len(baselist)):
        sys.stdout.write(baselist[n]+" "+baselist[n]+"\n")
        if beamwidth > 1:
            samp = random.sample(baselist,beamwidth-1)
            for m in range(len(samp)):
                sys.stdout.write(baselist[n]+" "+samp[m]+"\n")



