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


# Data downloader for the Zero Speech Challenge 2017.


# called on script errors
function failure { [ ! -z "$1" ] && echo "Error: $1"; exit 1; }

# basic argument checking
if [ -z $1 ]; then
    echo "Usage: $0 data_dir"
    exit 1
fi

data_dir=$1

echo "Dowloading data to $data_dir"

# mkdir -p $data_dir
# TODO !!!
