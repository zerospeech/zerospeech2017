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
# Data downloader for the Zero Speech Challenge 2017.
#

# basic argument checking
if [ -z $1 ]; then
    echo "Usage: $0 data_dir"
    exit 1
fi

# create data directory
data_dir=$1
mkdir -p $data_dir

# check if the wget version accept the option --show-progress
# source: goo.gl/JB0is8
wget --help | grep -q '\--show-progress' && \
  _PROGRESS_OPT="-q --show-progress" || _PROGRESS_OPT=""

echo "If you don't have the password, please register \
by sending us an email to : zerospeech2017@gmail.com"
# downloading data
wget ftp://129.199.81.135/datasets_2/ -P $data_dir \
     --user=challenge2017 --ask-password \
     -m -nH $_PROGRESS_OPT --cut-dirs=1
