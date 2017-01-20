#!/bin/bash
#
# Copyright 2016, 2017 Mathieu Bernard
#
# You can redistribute this file and/or modify it under the terms of
# the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

#
# Baseline replication for the Track1 of the Zero Speech Challenge 2017.
#
# Run a single lang/duration combination. Extract MFCC features with
# deltas and deltas-deltas. Evaluation them on the ABX
# discriminability task.
#

set -e
# equivalent to $(readlink -f $1) in pure bash (compatible with macos)
function realpath {
    pushd $(dirname $1) > /dev/null
    echo $(pwd -P)
    popd > /dev/null
}

# called on script errors
function failure { [ ! -z "$1" ] && echo "Error: $1"; exit 1; }


# path to the features extraction program
features_extraction=$(realpath "${BASH_SOURCE[0]}")/../src/extract_features/features_extraction_challenge.py
[ ! -f $features_extraction ] && failure "cannot find $features_extraction"

# MFCC configuration file
mfcc_config=$(realpath "${BASH_SOURCE[0]}")/../src/extract_features/mfcc_config.json
[ ! -f $mfcc_config ] && failure "cannot find $mfcc_config"

# evaluation program
eval_track1=$(realpath "${BASH_SOURCE[0]}")/../eval/eval_track1.py
[ ! -f $eval_track1 ] && failure "cannot find $eval_track1"

# njobs to use for evaluation
njobs=4


# basic argument checking
if [ ! $# -eq 4 ]; then
    echo "Usage: $0 data_dir output_dir lang duration"
    echo "  data_dir is the path to the downloaded challenge dataset"
    echo "  output_dir is the path to evaluation results"
    echo "  lang is either english, mandarin or french"
    echo "  duration is either 1, 10, 120"
    exit 1
fi
[ ! -d $1 ] && failure "$1 is not an existing directory"
[[ ! $3 == "english" && ! $3 == "mandarin" && ! $3 == "french" ]] && \
    failure "unknown language $3. Choose english, mandarin or french."
[[ ! $4 == 1 && ! $4 == 10 && ! $4 == 120 ]] && \
    failure "unknown duration. Choose 1, 10 or 120."


# parse input arguments
data_dir=$1
output_dir=$2
lang=$3
duration=${4}s  # append a s

# create output directory if not existing
mkdir -p $output_dir || failure "cannot create directory $output_dir"

# a file wrote by feature_extraction we remove at exit
trap "rm -f octave-workspace" EXIT


# compute features for old and new speakers


# input directory with wavs
current_data_dir=$data_dir/test/$lang/$duration
[ ! -d $current_data_dir ] && failure "cannot find $current_data_dir"

# output h5features file
output_file=$output_dir/${duration}.h5f
if [ -f $output_file ] ; then
    echo "overwritting $output_file"
    rm -f $output_file
fi

echo "features extraction for $lang $duration "
python $features_extraction $current_data_dir/*.wav -h5 $output_file -c $mfcc_config \
    || failure "cannot extract features from $current_data_dir"



# evaluate the features
current_output_dir=$output_dir/eval_$duration
mkdir -p $current_output_dir || failure "cannot create directory $current_output_dir"

$eval_track1 --h5 --njobs $njobs $lang ${duration::-1} -n 1 $data_dir $output_dir $current_output_dir \
    || failure "cannot evaluate $lang $duration"

exit 0
