# Zero Speech Challenge 2017

All you need to get started for the track 1 and track 2 of the
**[Zero Speech Challenge 2017](http://sapience.dec.ens.fr/bootphon/)**:

* Speech data download
* Evaluation software setup
* Baseline and topline replication

The setup procedure is described for Linux. It has been tested on
several distributions (Ubuntu 16.04, Debian Jessie and CentOS 6). It
should work as well on MacOS.


## Getting the hyper-training data

TODO Put online raw speech data and ABX task files.

TODO Write a *get_data.sh* with wget commands to the server. This will
create ./data/{lang1, lang2, etc...} folders with {train, test}
subfolders.


## General setup

* First of all install [git](https://git-scm.com/downloads)
  and [conda](http://conda.pydata.org/miniconda.html) on your system,
  if not already done.

* Create a dedicated Python2 virtual environment to isolate this code
  from the system wide Python installation.

        conda create --name zerospeech python=2

* Then clone this repository and go in its root directory

        git clone --recursive git@github.com:bootphon/zerospeech2017.git
        cd zerospeech2017

* **Do not forget to activate your virtual environment**

        source activate zerospeech


## Track 1: Unsupervised subword modeling

### Installation

Simply have a:

    ./track1/setup/setup_track1.sh

This installs the dependencies of the track 1 evaluation program
(ABXpy and h5features) from the `./track1/src` folder to your virtual
environment. To make sure the installation is correct, you can run
their tests:

    pytest ./track1/src


### Evaluation program


To do the evaluation, do:

    cd ./track1/eval
	./eval_track1.py /path/to/your/feature/folder/ length_of_the_files corpus /path/to/your/output/folder/ .

For example, for features extracted on 1s files for the chinese corpus :

	./eval1 /home/julien/data 1 chinese temp/


### Baseline replication

TODO Write a `track1/baseline.sh` just calling features_extraction and
eval_track1 with the baseline MFCC features.


### Topline replication

TODO Write a `track1/topline.sh`. This will
require [abkhazia](https://github.com/bootphon/abkhazia) -> posteriors on DNN



## Track 2: Spoken term discovery

TODO


## Licence

**Copyright 2016, 2017 LSCP Bootphon team**

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
