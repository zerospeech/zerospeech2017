#!/bin/bash

set -e

# including the input variables from the file config
. config

### creating lsh and feature files in data/cache
cd data
make
cd -
exit

### run discovery for parameters on the 'config' file
./run_disc   

### run Aren's clustering algorithm
./post_disc 

### evaluating the results 
mkdir -p ./out

python ../../bin/english_eval2.py -v ${EXPDIR}/${EXPNAME}/results/${MASTER_GRAPH}.class out                                                                 

