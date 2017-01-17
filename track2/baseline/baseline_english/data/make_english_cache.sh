#!/bin/bash 

set -e

## Zero Resource Speech Challenge 2017 

## CREATION OF CACHE FILES FOR ZRTOOLS

## build a cache with files that will be used by the plebdisc algorithm.
## It uses sox to preprocess the files the wav files: reduce the volume, 
## cut the files and setting the same sampling rate for all the files. 

## the input S is the size of the random projection in ZRTools and should
## be 32 or 64

S=$1
DIM=39

if [ -z "$S" ]; then
    echo "USAGE: " $0 " VALUE [VALUE=32,64]";
    exit 1;
fi

if [ "$S" != "64" ]; then
    echo "USAGE: " $0 " VALUE [VALUE=64]";
    exit 1;
fi

# testing for needed commands in this script 
for cmd in "sox" "feacalc" "standfeat" "lsh" ; do
    printf "%-10s" "$cmd"
    if hash "$cmd" 2>/dev/null; then 
        printf "OK\n"; 
    else 
        printf "missing\n"; 
        exit 1;
    fi
done

# directory with the VAD for each wav file
VAD_DIR=./vad
CACHE=./cache
LOC_TMP=$(pwd)

# output dirs
mkdir -p ${CACHE}/D${DIM}S${S}/feat
mkdir -p ${CACHE}/D${DIM}S${S}/lsh
mkdir -p ${CACHE}/D${DIM}S${S}/wav


tempdir=$(mktemp -d --tmpdir=${LOC_TMP});
echo "### " $tempdir     

# the random proj fie 
genproj -D $DIM -S $S -seed 1 -projfile ${CACHE}/proj_S${S}xD${DIM}_seed1

# trim 10 mins of waveforms
trim="trim 0 10:00"

# addapted from from ZRTools/script/generate_plp_lsh
function p_norm() {
    file_=$1;
    id=$(basename $file_ .wav);

    # Get audio into a 16-bit 8kHz wav file                               
    out_wav=${tempdir}/${id}.wav
    echo ">>>>>> doing sox";                                              
    sox -v 0.8 -t wav $file_ -t wav -e signed-integer \
        -b 16 -c 1 -r 8000 $out_wav $trim;                          

    ### Generate 39-d PLP (13 cc's + delta + d-deltas using ICSI feacalc) 
    echo ">>>>>> doing feacalc";                                          
    feacalc -plp 12 -cep 13 -dom cep -deltaorder 2 \
    -dither -frqaxis bark -samplerate 8000 -win 25 \
    -step 10 -ip MSWAVE -rasta false -compress true \
    -op swappedraw -o ${tempdir}/${id}.binary \
    ${tempdir}/${id}.wav;   

    echo ">>>>>> doing standfeat";
    standfeat -D $DIM -infile ${tempdir}/${id}.binary \
        -outfile ${tempdir}/${id}.std.binary \
        -vadfile ${VAD_DIR}/${id}

    echo ">>>>>> doing lsh";
    lsh -D $DIM -S $S -projfile ${CACHE}/proj_S${S}xD${DIM}_seed1 \
        -featfile ${tempdir}/${id}.std.binary \
        -sigfile ${tempdir}/${id}.std.lsh64 -vadfile ${VAD_DIR}/${id}

    cp -rf ${tempdir}/${id}.std.lsh64 ${CACHE}/D${DIM}S${S}/lsh
    cp -rf ${tempdir}/${id}.std.binary ${CACHE}/D${DIM}S${S}/feat

}

# do all the file
for i in $(cat english.lst); do p_norm $i; done

rm -rf $tempdir
exit 1;

