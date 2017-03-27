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


# Setup script for the track 1 of the Zero Speech Challenge 2017. This
# script assumes you have created (and activated) a dedicated Python2
# environment. If this is not the case, have a:
#
#   conda create -n zerospeech python=2
#   source activate zerospeech


# equivalent to $(readlink -f $1) in pure bash (compatible with macos)
function realpath {
    pushd $(dirname $1) > /dev/null
    echo $(pwd -P)
    popd > /dev/null
}

# called on script errors
function failure { [ ! -z "$1" ] && echo "Error: $1"; exit 1; }

# go to this script directory, restore current directory at exit
trap "cd $(pwd)" EXIT
cd $(realpath "${BASH_SOURCE[0]}")

# setup the Python environment with conda and pip
conda install --yes --file requirements_conda.txt \
    || failure "cannot install from requirements_conda.txt"
pip install -r requirements_pip.txt \
    || failure "cannot install from requirements_pip.txt"

# setup tde 
cd ../src/tde
python setup.py install --force || failure "cannot install tde"
# TODO: change setup.py to point to the bin_dir
bin_dir=../../bin/
mkdir -p ${bin_dir}
install bin/english_eval2.py ${bin_dir} || failure "cannot copy tde - english_eval2.py"
install bin/french_eval2.py ${bin_dir}  || failure "cannot copy tde - french_eval2.py"
install bin/mandarin_eval2.py ${bin_dir} || failure "cannot copy tde - mandarin_eval2.py"
cp -rf bin/resources  ${bin_dir}        || failure "cannot copy tde - bin/resources"
bunzip2 ${bin_dir}/resources/*.bz2             || failure "cannot copy tde - bin/resources"
cd -

# creating links to the evaluation scripts
ln -s ../bin/english_eval2.py ../eval/english_eval2.py  || failure "cannot copy tde - english_eval2.py"  
ln -s ../bin/french_eval2.py ../eval/french_eval2.py    || failure "cannot copy tde - french_eval2.py"
ln -s ../bin/mandarin_eval2.py ../eval/mandarin_eval2.py|| failure "cannot copy tde - mandarin_eval2.py"

# setup ZRTools 
cd ../src/ZRTools
make  || failure "cannot install ZRTools"
install plebdisc/build_index ${bin_dir} || failure "cannot copy ZRTools - build_index" 
install plebdisc/genproj ${bin_dir}     || failure "cannot copy ZRTools - genproj"
install plebdisc/lsh ${bin_dir}                  || failure "cannot copy ZRTools - lsh"
install plebdisc/plebdisc ${bin_dir}             || failure "cannot copy ZRTools - plebdisc"
install plebdisc/plebkws ${bin_dir}              || failure "cannot copy ZRTools - plebkws"
install plebdisc/rescore_singlepair_dtw ${bin_dir} || failure "cannot copy ZRTools - rescore_singlepair_dtw"
install plebdisc/standfeat ${bin_dir}            || failure "cannot copy ZRTools - standfeat"
cd -

# setup icsi (feacal)
cd ../src/icsi-scenic-tools-20120105
./configure || failure "cannot configure ../src/icsi-scenic-tools-20120105" 
make || failure "cannot install feacal"
install ./feacalc-0.92/feacalc ${bin_dir} || failure "cannot copy featcalc - scripts/" 
cd -

