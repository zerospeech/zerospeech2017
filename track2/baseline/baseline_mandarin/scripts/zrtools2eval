''' zrtools2eval : convert result files from ZRTools in the input format used by the Challenge 2017
'''

from __future__ import print_function, division
from optparse import OptionParser

# From ZRTools/readme.md:
# master_graph.nodes: Pseudoterm tokens, one per line; the first three
# columns specify the input file, start frame, and end frame (frames =
# seconds*100)

# master_graph.dedups: Pseudoterm cluster definition, one line per
# cluster, each consisting of a list of node IDs that corresponds to the
# line number in the .nodes file

# master_graph.feats: Bags-of-pseudoterms sparse vector definition
# (SVMLight format), one line per input file.  The feature-id is the
# pseudoterm ID that corresponds to the line number of the .dedups file.

parser = OptionParser()
parser.add_option("--nodes", help="nodes input file", dest="nodesfile")
parser.add_option("--dedups", help="dedups input file", dest="dedupsfile")
parser.add_option("--output", help="eval class output file", dest="outfile")
(options, args) = parser.parse_args()

nodesfile = options.nodesfile
dedupsfile = options.dedupsfile
outfile = options.outfile

if nodesfile is None:
    parser.error('nodes file not given')

if dedupsfile is None:
    parser.error('dedups file not given')

# Decode nodes file
nodes_ = dict()
with open(nodesfile) as nodes:
    for n, node in enumerate(nodes, start=1):
        try:
            wavfile, start, end  = node.split()[:3]
            nodes_[n] = [wavfile, float(start)/100.0, float(end)/100.0] 
        except:
            raise 

# decode dedups file
dedups_ = list()
with open(dedupsfile) as dedups:
    for dedup in dedups:
        try:
            dedups_.append([int(n) for n in dedup.split() ])  
        except:
            raise

# creating the output class used by eval
t_ = ''
for n, class_ in enumerate(dedups_, start=1):
    t_ += 'Class {}\n'.format(n)
    for element in class_:
        file_, start_, end_ = nodes_[element]
        t_ += '{} {:.3f} {:.3f}\n'.format(file_, start_, end_)
    t_ += '\n'

# stdout or save to file file
if outfile is None:
    print(t_)
else:
    with open(outfile, 'w') as output:
       output.write(t_) 


