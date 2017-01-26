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
# Pipeline for entire baseline replication (all languages, all
# durations). This simply call baseline_one.sh for each
# language/duration combination.
#


# equivalent to $(readlink -f $1) in pure bash (compatible with macos)
function realpath {
    pushd $(dirname $1) > /dev/null
    echo $(pwd -P)
    popd > /dev/null
}

# called on script errors
function failure { [ ! -z "$1" ] && echo "Error: $1"; exit 1; }


# path to the baseline_one.sh script
baseline_one=$(realpath "${BASH_SOURCE[0]}")/baseline_one.sh
[ ! -f $baseline_one ] && failue "cannot find $baseline_one"


# basic argument checking
if [ ! $# -eq 2 ]; then
    echo "Usage: $0 data_dir output_dir"
    echo "  data_dir is the path to the downloaded challenge dataset"
    echo "  output_dir is the path to evaluation results"
    exit 1
fi
[ ! -d $1 ] && failure "$1 is not an existing directory"

# parse input arguments
data_dir=$1
output_dir=$2


for lang in english mandarin french
do
    for duration in 1 10 120
    do
        $baseline_one $data_dir $output_dir/$lang $lang $duration \
            || failure "cannot compute baseline for $lang ${duration}s"
	# output result in csv
	echo "$lang ${duration}s" >> $output_dir/result.csv
	cat $output_dir/$lang/eval_${duration}s/results.txt \
		| awk -F ' ' '{if ($2 ~ /[0-9]/) print $1 " " (1-$2)*100}' >> $output_dir/result.csv
    done
done

exit 0
