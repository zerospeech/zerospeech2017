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
#
# TODO Update the script for topline/baseline replication setup
# (features_extraction, abkhazia)
# TODO Why are we installing spectral (and so oct2py) in
# requirements_pip.txt ?


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

# setup h5features and ABXpy
cd ../src/h5features; python setup.py install || failure "cannot install h5features"; cd -
cd ../src/ABXpy; make install || failure "cannot install ABXpy"; cd -
