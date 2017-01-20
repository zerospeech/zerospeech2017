**Please register by sending us an email to zerospeech2017@gmail.com**

**We will keep you informed for changes in code**

# Zero Speech Challenge 2017

All you need to get started for the track 1 and track 2 of the
**[Zero Speech Challenge 2017](http://www.zerospeech.com)**:

* Train and test data download: raw speech, ABX tasks
* Evaluation software setup
* Baseline and topline replication

The setup procedure is described for Linux. It has been tested on
several distributions (Ubuntu 16.04, Debian Jessie and CentOS 6). It
should work as well on MacOS.


## Registration 

* In order to participate to this challenge and have access to the
  datasets, you have to send an email to : zerospeech2017@gmail.com

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


## Getting the development dataset (hyperparameter training set)

* To have the password to download the data, you first have to
  register yourself by sending an email to : 
	zerospeech2017@gmail.com

* Dowload the whole challenge hyper-training dataset using the
  `download_data.sh` script :

        ./download_data.sh ./data

* The dataset is about 34 GB large, so it will take a while to
  download.

* Once downloaded, the `./data` directory has the following structure:

        train/
            {english, mandarin, french}/
                *.wav
        test/
            {english, mandarin, french}/
                {1s, 10s, 120s}/
                    *.wav
                    across.abx
                    within.abx

* All the wav files are sampled at 16kHz on 16 bits.

* The `*.abx` files are
  [ABX task files](https://abxpy.readthedocs.io/en/latest/FilesFormat.html#task-file)
  required for the evaluation.

* The train corpora have been cut by speakers, and the speaker
  identity is the name of its wav file (e.g. speaker B07 is in the
  file `B07.wav`).

* The test corpora have been cut into small files of controlled size
  (1s, 10s or 120s). 
  Speaker identification is *not* available for test (wav
  files randomly named).


## Track 1: Unsupervised subword modeling

### Installation

* With your virtual environment activated, simply have a:

        ./track1/setup/setup_track1.sh

  This installs the dependencies of the track 1 evaluation program,
  baseline and topline replication from the `./track1/src` folder to
  your virtual environment. Those dependencies are:

  * [ABXpy](https://github.com/bootphon/ABXpy) for evaluation
  * [h5features](https://github.com/bootphon/h5features) for data storage
  * [abkhazia](https://github.com/bootphon/abkhazia) for topline (optional)

* To make sure the installation is correct, you can run the tests:

        pytest ./track1/src


### Features file format

* Our evaluation system requires that your unsupervised subword modeling
  system outputs a vector of feature values for each frame in the following format:

        <time> <val1>    ... <valN>
        <time> <val1>    ... <valN>

  Exemple:

        0.0125 12.3 428.8 -92.3 0.021 43.23
        0.0225 19.0 392.9 -43.1 10.29 40.02
        ...

* The time is in seconds. It corresponds to the center of the frame of
  each feature. In this example, there are frames every 10ms and the
  first frame spans a duration of 25ms starting at the beginning of
  the file, hence, the first frame is centered at .0125 seconds and
  the second 10ms later. It is not required that the frames be
  regularly spaced, in fact the only requirement is that the timestamp
  of frame *n+1* is strictly larger than the timestamp of frame
  *n*. The frame timestamps are used by the evaluation software to
  determine which features correspond to a particular triphone among
  the sequence of features for a whole sentence on the basis of manual
  phone-level alignments for that sentence.

* For each wav in the test set (e.g. `data/test/mandarin/1s/aghsu09.wav`),
  an ASCII features file with the same name
  (e.g. `features/test/mandarin/1s/aghsu09.fea`) as the wav should be
  generated in the same subdirectories logic *<lang>/<length>/<name>*


### Evaluation program

* The Track 1 evaluation programs are `./track1/eval/eval_track1.py`. The
  detail of arguments is given by the `--help` option:

        cd ./track1/eval
        ./eval_track1.py --help

* For example this command will evaluate features extracted on 1s files
  for the mandarin corpus:

	    ./eval_track1.py mandarin 1 /path/to/feature/folder/ /path/to/output/folder/

* The input feature folder must contain a collection of feature files
  as described above, one file per wav files in the test corpus.

* The evaluation result is an aggregated ABX discriminability. An
  example of output file, called `results.txt` is:

        task	score
        across_talkers:	0.757
        within_talkers:	0.868


### Using your own distance in evaluation

To see how it is possible to provide your own distance, let us show
first how it is possible to obtain the default DTW+cosine distance
using the `-d` option. The distance function used by default
(DTW+cosine) is defined in the python script
`./track1/eval/distance.py` by the function named distance. So calling
the `eval_track1.py` executable from the `./track1/eval` folder with
the option `-d ./distance.distance` will reproduce the default
behavior.

Now to define your own distance function you can for example copy the
file `./track1/eval/distance.py` in directory *dir* somewhere on your
system, modify the distance function definition to suit your needs and
call `eval_track1.py` with the option `-d dir/distance.distance`.

You will see that the `distance.py` script begins by importing three
other python modules, one for DTW, one for cosine distance and one for
Kullback-Leibler divergence. The cosine and Kullback-Leibler modules
are located in folder `./track1/src/ABXpy/distances/metrics` and
implement frame-to-frame distance computations in a fashion similar to
the
[scipy.spatial.distance.cdist](http://docs.scipy.org/doc/scipy/reference/generated/scipy.spatial.distance.cdist.html#scipy.spatial.distance.cdist) function
from the scipy python library.

The DTW module is also located in the folder
`./track1/src/ABXpy/distances/metrics` but as a static library
(`dtw.so`) compiled from the cython source file
`./track1/src/ABXpy/distances/metrics/install/dtw.pyx` for efficiency
reasons. You can use our optimized DTW implementation with any
frame-to-frame distance function with a synopsis like the
*scipy.spatial.distance.cdist* function by modifying appropriately
your copy of `distance.py`. You can also replace the whole distance
computation by any python or cython module that you designed as long
as it has the same input and output format than the the distance
function in the `distance.py` script.


### Baseline replication

The track 1 baseline is to run the evaluation on MFCC features with
delta and delta-delta (39 dimensions) extracted directly from the test
corpora, whithout any learning.

You can replicate the baseline (i.e. extract MFCCs and evaluate them)
on the entire test dataset with the command:

    ./track1/baseline/baseline_all.sh ./data ./track1/baseline/results

In this example, `./data` is the path to the downloaded challenge
dataset and `./track1/baseline/results` is created output directory
with baseline features and evaluation results.


### Topline replication

The topline consists in training an HMM-GMM phone recogniser with speaker adapation
on the training set and extracting from the test set a frame-by-frame posteriorgram. 
This is done kaldi script (`track1/topline.sh`).
It requires [abkhazia](https://github.com/bootphon/abkhazia). 

THIS IS NOT YET AVAILABLE
When this topline is available, we will email you about the update (available with git-pull).

## Track 2: Spoken term discovery

**The baseline pipeline and the evaluation kit are build for the first 10 minutes of all test data.
Results of the baselines are still preliminary**

**Please register to keep you updated of changes on the results of the baseline and evaluation code**

Track 2 evaluation is done on the training set only. This may seem strange, but remember this is unsupervised
training (the test set contains files that are not cut in a way that is appropriate for Track 2). 

NOTICE: The Tack 2 evaluation pipeline is as of today not in its final phase. Right now, the evaluation scripts
only evaluate a small percentage of the files and the baseline and toplines are not finished.
You can still work on the data, and the updated evaluation will be notified (and available through a git pull).

### Installation

* With your virtual environment activated, simply have a:

        ./track2/setup/setup_track2.sh

  This installs the dependencies of the track 2 evaluation program,
  baseline and topline replication from the `./track2/src` folder to
  your virtual environment. Those dependencies are:

  * [ZRTools](https://github.com/bootphon/zerospeech2017/tree/master/track2/src/ZRTools) for baseline
  * [tde](https://github.com/bootphon/tde) for evaluation
  * [feacalc](https://www1.icsi.berkeley.edu/~dpwe/projects/sprach/sprachcore.html) for feature computation
  
### Output format 

  The spoken word discovery system should output an ASCII file listing
   the set of fragments that were found with the following format:

     Class <classnb>
     <filename> <fragment_onset> <fragment_offset>
     <...>
     <filename> <fragment_onset> <fragment_offset>
     <NEWLINE>
     Class <classnb>
     <filename> <fragment_onset> <fragment_offset>

  example:

     Class 1
     dsgea01   1.238  1.763 
     dsgea19   3.380  3.821
     reuiz28  18.036 18.537


     Class 2
     zeoqx71   8.389  9.132
     ...etc...
  Note: the onset and offset are in seconds. If your system only does
  matching and not clustering, your classes will only have two
  elements each. If your system does not only matching, but also 
  clustering and parsing, the fragments found will cover the entirety
  of the files, and there may be classes with only one element in it
  (the remainder of lexical-based segmentation).

### Evaluation program
mandarin_eval2.py
* The Track 2 evaluation program are `./track2/eval/english_eval2.py`, 
`./track2/eval/french_eval2.py` and `./track2/eval/mandarin_eval2.py`. The
  detail of arguments is given by the `--help` option:

        cd ./track2/bin
        ./mandarin_eval2.py --help

* For example this command will evaluate the output for the
  for the Mandarin corpus:

	    ./mandarin_eval2.py mandarin.classes result_dir/

* To run the evaluation on multiple cores, use the j-flag. Evaluation
  runtime and memory usage are also strongly dependent on the 
  particulars of the input file. It is not usefule to use more than
  10 cores (each parallel job will do one of the 10 subsampling folds)

* The output directory will contain one file each for the above 
  described measures, with scores for both cross-speaker and 
  within-speaker performance. The directory will also contain a file 
  called ``VERSION_$'' indicating the version of the evaluation code 
  that was used. Please make sure to report that number in your report. 
  The version number can also be obtained by:

  	$ python ./track2/eval/eval_track2.py -V

### Baseline replication

THIS IS NOT YET AVAILABLE

When available you will be able to replicate the baseline on one of the corpus with the command:

    bash /track2/baseline/baseline_french/run.sh 

## Troubleshooting

### Troubles with track 1 or track 2

Please open an
issue [here](https://github.com/bootphon/zerospeech2017/issues).


### Troubles with software dependencies

If you have problems with ABXpy, h5features or another of our tools,
please reefer to their related github page on
the [Bootphon repository](https://github.com/bootphon).


### Multiprocessing.py

The parallelisation of our program relies on a module from Python's
standard library called *multiprocessing.py* which can be a bit
unstable. If you experience problems when running the evaluation, try
requiring only one CPU to avoid using this module altogether.


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
