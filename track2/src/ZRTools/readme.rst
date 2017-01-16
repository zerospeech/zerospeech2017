======================================================================
ZRTools: Zero-Resource Speech Discovery, Search and Evaluation Toolkit
======================================================================

Contributors
============
- Aren Jansen 
- Ben Van Durme
- Greg Sell

References
==========

Please cite the relevant reference in any publications that use these
tools:

Evaluation (samediff)

- Michael A. Carlin, Samuel Thomas, Aren Jansen, and Hynek
  Hermansky. Rapid Evaluation of Speech Representations for Spoken Term
  Discovery. Proceedings of Interspeech, 2011. 

Discovery (plebdisc)

- Aren Jansen and Benjamin Van Durme. Efficient Spoken Term Discovery
  Using Randomized Algorithms. Proceedings of ASRU, 2011.

Search (plebkws)

- Aren Jansen and Benjamin Van Durme. Indexing Raw Acoustic Features for
  Scalable Zero Resource Search. Proceedings of Interspeech, 2012. 

Installation
============
1. Run "make" in the base directory.  This will build all executables in
   the toolkit.

2. Install dependencies (see following section for details).

3. Edit the "config" file to be compatible with your environment.  

4. If you wish to use SGE, you may also need to modify
scripts/split_submit_array to work in your SGE environment.

Dependencies
============

feacalc
-------
The tools come set up to use feacalc (from ICSI's Sprachcore toolkit)
for feature extraction (PLP by default).  This package is no longer
maintained and may be difficult to install.  If you wish to use another
feature extraction tool, you will need to modify
scripts/generate_plp_lsh accordingly.  Note that the discovery and
search tools take raw 4-byte float feature files as input.

sph2pipe
--------
This is available from LDC.

SoX: SoundExchange
------------------
This is a standard tool available with all linux distributions.

Main Scripts
============
There are three driver scripts in the base directory:

run_disc 
--------
Given a list of audio files (.sph or .wav), generates PLP features and
corresponding LSH signatures for each, and then runs the plebdisc
discovery tool on each pair of LSH files. This can either be run on the
local machine or run using SGE.  Usage is provided by the script if no
arguments are given.

post_disc
---------
Post-processes the run_disc discovery output to generate
pseudo-term clusters and bags-of-pseudoterms representations for each
audio file (this is done on the local machine, no SGE). Usage is
provided by the script if no arguments are given.

run_samediff
------------
Run the same-different zero resource evaluation code
(see Carlin et al, Interspeech 2011).  Before running this, you must
supply a wordlist and generate a feature file for token in the
wordlist. Usage is provided by the script if no arguments are given. 

Discovery Usage Example
=========================

Below is a simple example of running the discovery tool on a small
subset of the DAPS corpus
(https://ccrma.stanford.edu/~gautham/Site/daps.html).  The file list we
use is given in scripts/daps.lst.  After following Installation
instructions, modify the daps.lst file to point the corresponding files
on your system.  Then run the following commands (assuming no SGE, use
sge in place of nosge to use SGE):

1. Run discovery on all pairs of files in the daps.lst input list::

    ./run_disc nosge scripts/daps.lst

2. Run pseudoterm clustering with DTW Similarity threshold 0.88::

    ./post_disc daps 0.88

3. Generate pseudoterm example wav files::

    ./scripts/pt_wavs daps   

The second post_disc argument is the match DTW similarity threshold; for
the 39D PLP features we are using, a threshold of 0.88 will generally
allow some cross-speaker matches (increasing it to 0.9 or higher will
limit to mostly within-speaker matches but will let through less false
alarms).  If you change the front end, all bets are off as to what this
threshold means.

The discovery outputs will end up in $EXPDIR/daps/results/, and includes
the following files:

- master_graph.nodes: Pseudoterm tokens, one per line; the first three
  columns specify the input file, start frame, and end frame (frames =
  seconds*100) 

- master_graph.dedups: Pseudoterm cluster definition, one line per
  cluster, each consisting of a list of node IDs that corresponds to the
  line number in the .nodes file

- master_graph.feats: Bags-of-pseudoterms sparse vector definition
  (SVMLight format), one line per input file.  The feature-id is the
  pseudoterm ID that corresponds to the line number of the .dedups file.

The pseudoterm wav files will end up in $EXPDIR/daps/tmpwav (one .wav
for each pseudoterm consisting of 10 random snippets from the cluster).

Executable Summary
==================

plebdisc/
---------

- build_index: generate a RAILS index for a collection of feature files

- genproj: generate LSH project matrix

- lsh: extract LSH signatures from a feature file

- plebdisc: discovery repetitions between a pair of feature files

- plebkws: query-by-example keyword search using a RAILS index

- rescore_singlepair_dtw: rescore matches to use exact DTW similarity
  computed from specified feature files (useful for replacing LSH-approx
  sims or using different features for rescoring).

- standfeat: apply per-dimension mean-variance normalization to a
  feature file (optionally only computing normalization stats in speech
  regions)

samediff/
---------

- compute_distrib: compute the similarity score distributions and
  generate performance metrics

- wordsim: compute all pairs of DTW similarities between pairs of word
  examples 


srailsdisc/
-----------

- genproj: same as plebdisc version

- lsh: improved over plebdisc version, now takes arbitrary signature
  length that is multiple of 8

- srailsdisc: S-RAILS based discovery code, not published, not
  documented, not fully tested, and not supported


