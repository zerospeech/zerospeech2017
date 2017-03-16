#!/usr/bin/env python
#
# Copyright 2016, 2017 Julien Karadyi
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
"""Evaluation program for the Zero Speech Challenge 2017

Track 1: Unsupervised subword modeling

"""

import ast
import argparse
import ConfigParser
import h5py
import os
import numpy as np
import pandas
import pickle
import sys
import warnings
import random 

from tables import DataTypeWarning
from tables import NaturalNameWarning

import ABXpy.task as ABXtask
import ABXpy.distances.distances as distances
import ABXpy.distances.metrics.cosine as cosine
import ABXpy.distances.metrics.dtw as dtw
import ABXpy.score as score
import ABXpy.analyze as analyze
from ABXpy.misc import any2h5features


# version of this script
VERSION = "0.1"

# directory where this script is stored
CURDIR = (os.path.dirname(sys.executable) if getattr(sys, 'frozen', False) else
          os.path.dirname(os.path.realpath(__file__)))

# distance function to call with the --kl option
KL_DISTANCE_MODULE = os.path.join(CURDIR, 'distance.kl_divergence')

# configuration file
CONFIG_FILE = os.path.join(CURDIR, 'eval_track1.cfg')


def loadfeats(path):
    aux = np.genfromtxt(path)
    time = aux[:, 0]
    features = aux[:, 1:]
    return {'time': time, 'features': features}


class Discarder(object):
    def __init__(self):
        self.filt = ("Exception RuntimeError('Failed to retrieve old "
                     "handler',) in 'h5py._errors.set_error_handler' ignored")
        self.oldstderr = sys.stderr
        self.towrite = ''

    def write(self, text):
        self.towrite += text
        if '\n' in text:
            aux = []
            lines = text.split('\n')
            for line in lines:
                if line != self.filt and line != " ignored":
                    aux.append(line)
            self.oldstderr.write('\n'.join(aux))
            self.towrite = ''

    def flush(self):
        self.oldstderr.flush()

    def __exit__(self):
        self.oldstderr.write(self.towrite)
        # self.oldstderr.flush()
        sys.stderr = self.oldstderr


def modified(filepath, mtime):
    return not os.path.exists(filepath) or (mtime > os.path.getmtime(filepath))


def dtw_cosine_distance(x, y, normalized):
    return dtw.dtw(x, y, cosine.cosine_distance, normalized)


def parseConfig(configfile):
    taskslist = []
    config = ConfigParser.ConfigParser()
    assert os.path.exists(configfile), \
        'config file not found {}'.format(configfile)

    config.read(configfile)
    assert config.has_section('general'), \
        'general section missing in config file'

    general_items = dict(config.items('general'))
    sections = [section for section in config.sections()
                if section != 'general']
    for section in sections:
        task_items = dict(config.items(section))
        task_items['section'] = section
        for item in general_items:
            if item in task_items:
                warnings.warn(
                    'general config setting redefined in the task, '
                    'the task specific one will be used ({}: {})'
                    .format(item, task_items[item]), UserWarning)
            else:
                task_items[item] = general_items[item]
        taskslist.append(task_items)
    return taskslist


def checkIO(taskslist):
    for task_items in taskslist:
        # assert 'on' in task_items, 'missing ON argument for task {}'.format(
        #     task['section'])
        # assert 'taskfile' in task_items
        assert 'distancefile' in task_items
        assert 'scorefile' in task_items
        assert 'analyzefile' in task_items
        assert 'outputfile' in task_items


def lookup(attr, task_items, default=None):
    if attr in task_items:
        return task_items[attr]
    else:
        return default


def changed_task_spec(taskfile, on, across, by, filters, regressors, sampling):
    try:
        taskstr = ' '.join([str(x) for x in [
            'on', on, 'across', across, 'by', by,
            'filters', filters, 'regressors', regressors,
            'sampling', sampling] if x])

        with h5py.File(taskfile) as f:
            return f.attrs.get('done') and (
                not (f.attrs.get('task') == taskstr))
    except:
        return True


def h5isdone(path):
    try:
        with h5py.File(path) as f:
            return f.attrs.get('done')
    except:
        return False


def changed_distance(distancefile, distancefun):
    try:
        with h5py.File(distancefile) as fh:
            return not fh.attrs.get('distance') == pickle.dumps(distancefun)
    except:
        return True


def getmtime(path):
    try:
        return float(os.path.getmtime(path))
    except:
        return float('Inf')


def nonesplit(string):
    if string:
        return string.split()
    else:
        return None


def tryremove(path):
    try:
        os.remove(path)
    except:
        pass


def avg(filename, task):
    task_type = lookup('type', task)
    df = pandas.read_csv(filename, sep='\t')
    if task_type == 'across':
        # aggregate on context
        groups = df.groupby(
            ['speaker_1', 'speaker_2', 'phone_1', 'phone_2'], as_index=False)
        df = groups['score'].mean()
    elif task_type == 'within':
        arr = np.array(map(ast.literal_eval, df['by']))
        df['speaker']  = [e for e, f, g in arr]
        df['context'] = [f for e, f, g in arr]
        #del df['by']

        # aggregate on context
        groups = df.groupby(['speaker', 'phone_1', 'phone_2'], as_index=False)
        df = groups['score'].mean()
    else:
        raise ValueError('Unknown task type: {0}'.format(task_type))

    # aggregate on talker
    groups = df.groupby(['phone_1', 'phone_2'], as_index=False)
    confusion_mat = df.pivot_table(index='phone_1', columns='phone_2', values='score')
    df = groups['score'].mean()
    average = df.mean()[0]

    return (average,confusion_mat)

def sampleitems(database, threshold, output_dir):
    ''' Reduce the size of the item file by removing phones that occur 
    too much. This helps having a faster task computation. '''
   
    print 'Reducing the item file size'

    df = pandas.read_csv(database, sep=' ')
    
    # aggregate on triphones + spkr and put a threshold
    groups=df.groupby(['#phone','prev-phone','next-phone','speaker']).count()
    
    rows_to_trim=groups[groups['#file']>threshold]
    temp_csv=os.path.join(output_dir,'temp.csv')
    
    # not really pretty trick to "reset" the correct column names
    rows_to_trim.to_csv(temp_csv,sep='\t')
    trim = pandas.read_csv(temp_csv,sep='\t')
    os.remove(temp_csv)
    
    result= pandas.merge(df,trim,how='left', on=['#phone','prev-phone',
        'next-phone','speaker'])
    result.columns=['#file','onset','offset','#phone','prev-phone',
            'next-phone','speaker','count1','count2','count3']
    result=result.drop(result[result.count1>0].index)

    # delete the superfluous columns
    del result['count1']
    del result['count2']
    del result['count3']
    result.to_csv(os.path.join(output_dir,'temp.item'),sep=' ',index=False)

    return os.path.join(output_dir,'temp.item')


def featureshaschanged(feature_folder, feature_file):
    return True


def makedirs(listfiles):
    for f in listfiles:
        pardir = os.path.dirname(f)
        if not os.path.isdir(pardir):
            try:
                os.makedirs(pardir)
            except:
                sys.stderr.write(
                    'Could not create directories along path for file {}\n'
                    .format(os.path.realpath(f)))
                raise


def fullrun(task, database, feature_folder, h5, no_task, corpus,
            on, across, by, verbose, distance,
            outputdir, normalized,  doall=True, ncpus=None):
    print("Processing task {}".format(task['section']))

    feature_file = os.path.join(outputdir, lookup('featurefile', task))

    try:
        if distance:
            distancepair = distance.split('.')
            distancemodule = distancepair[0]
            distancefunction = distancepair[1]
            path, mod = os.path.split(distancemodule)
            sys.path.insert(0, path)
            distancefun = getattr(__import__(mod), distancefunction)
        else:
            distancemodule = lookup(
                'distancemodule', task,
                os.path.join(CURDIR, 'distance'))
            distancefunction = lookup('distancefunction', task, 'distance')
            path, mod = os.path.split(distancemodule)
            sys.path.insert(0, path)
            distancefun = getattr(__import__(mod), distancefunction)
    except:
        sys.stderr.write('distance not found\n')
        raise

    task_file = os.path.join(outputdir, lookup('taskfile',task))
    distance_file = os.path.join(outputdir, lookup('distancefile', task))
    scorefilename = os.path.join(outputdir, lookup('scorefile', task))

    #taskfilename = os.path.join(
    #    data_folder, 'train', corpus,
    #    '{}_{}.abx'.format(corpus,task))

    # # taskfilename = os.path.join(CURDIR, lookup('taskfile', task))
    # taskname = os.path.join(
    #     lookup('taskdir', task), '{}/{}s_{}_{}.abx'.format(
    #         corpus, file_sizes, distinction, lookup('type', task)))
    # taskfilename = os.path.abspath(os.path.join(CURDIR, taskname))
    #print('Task file is {}'.format(taskfilename))
    #assert os.path.isfile(taskfilename), 'Task file unknown'

    analyzefilename = os.path.join(outputdir, lookup('analyzefile', task))
    # on = lookup('on', task)
    # across = nonesplit(lookup('across', task))
    # by = nonesplit(lookup('by', task))
    # filters = lookup('filters', task)
    # regressors = lookup('regressors', task)
    # sampling = lookup('sampling', task)
    if not ncpus:
        ncpus = int(lookup('ncpus', task, 1))

    makedirs([feature_file, distance_file, scorefilename, analyzefilename])

    # tasktime = getmtime(taskfilename)
    # featuretime = getmtime(feature_file)
    # distancetime = getmtime(distance_file)
    # scoretime = getmtime(scorefilename)
    # analyzetime = getmtime(analyzefilename)
    # featfoldertime = max([getmtime(os.path.join(feature_folder, f))
    #                       for f in os.listdir(feature_folder)])

    # Preprocessing
    if not h5:
        try:
            print("Preprocessing... Writing the features in h5 format")
            tryremove(feature_file)
            any2h5features.convert(
                feature_folder, h5_filename=feature_file, load=loadfeats)
            # featuretime = getmtime(feature_file)
            with h5py.File(feature_file) as fh:
                fh.attrs.create('done', True)
        except:
            sys.stderr.write('Error when writing the features from {} to {}\n'
                             'Check the paths availability\n'
                             .format(os.path.realpath(feature_folder),
                                     os.path.realpath(feature_file)))
            # tryremove(feature_file)
            raise
    else:
        feature_file = os.path.join(
            feature_folder, '{}.h5f'.format(corpus))

    # computing
    try:
        if no_task :
            print("Using task file previously computed")
        else:
            print("Computing task file")
            
            tryremove(task_file)
            mytask = ABXtask.Task(database, on, across, by, verbose )
            mytask.generate_triplets(task_file)
            tryremove(os.path.join(outputdir,'temp.item'))

        print("Computing the distances")
        tryremove(distance_file)
        distances.compute_distances(
            feature_file, '/features/', task_file,
            distance_file, distancefun, normalized = normalized , n_cpu=ncpus)

        tryremove(scorefilename)
        print("Computing the scores")
        score.score(task_file, distance_file, scorefilename)

        tryremove(analyzefilename)
        print("Collapsing the results")
        analyze.analyze(task_file, scorefilename, analyzefilename)

        return avg(analyzefilename, task)
    # except Exception as e:
    #     sys.stderr.write('An error occured during the computation\n')
    #     raise e
    finally:
        tryremove(distance_file)
        tryremove(scorefilename)
        tryremove(analyzefilename)
        if not h5:
            tryremove(feature_file)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    # parser.add_argument(
    #     '-c', '--config', default=CONFIG_FILE,
    #     help='config file, default to %(default)s')

    parser.add_argument(
        'corpus', choices=['english', 'mandarin', 'french'],
        help='test corpus you are evaluating your features on')

    parser.add_argument(
        '-database', metavar='<database>',
        help='file containing all the items of train dataset.'
        'If --no_task enabled, argument is not required.')

    parser.add_argument(
        'features', metavar='<feat-dir>',
        help='input directory containing the feature to evaluate')

    parser.add_argument(
        'output', metavar='<output-dir>',
        help='output directory used for intermediate files and results')
    parser.add_argument(
        '-j', '--njobs', metavar='<int>', type=int, default=1,
        help='number of cpus to use, default to %(default)s')

    parser.add_argument(
        '--h5', action='store_true',
        help='enable if the inputs at already in h5 format.')
    
    parser.add_argument(
        '-n', '--normalized',type=int, default=None,
        help='if using DTW distance in ABX score computation,'
            'put to -n 1 to normalize the DTW distance by the'
            'length of the DTW path, or put to -n 0 to just '
            'sum the cost along the DTW path. Common choice '
            ' is -n 1')
    parser.add_argument(
        '-v', '--verbose', action="store_true",
        help='display more messages to stdout')

    # taks options are mutually exclusive
    task_group = parser.add_argument_group(
        'task options').add_mutually_exclusive_group()
    task_group.add_argument(
        '-sample', type=int,
        help=' create the ABX task on a sub-sample of all the'
        'triplets available. Put a threshold on the number of '
        'occurences used for each triplet. Suggested '
        'threshold : 7 .')
    task_group.add_argument(
        '--no_task',action='store_true',
        help='enable if you already computed the task and want to use it.'
        'Useful since the task computation is the longest step of the '
        'evaluation.')

    # distance options are mutually exclusive
    group = parser.add_argument_group(
        'distance options').add_mutually_exclusive_group()

    group.add_argument(
        '-d', '--distance', metavar='<distance>',
        help='distance module to use (distancemodule.distancefunction), '
        'default to dtw cosine distance')

    group.add_argument(
        '-kl', action='store_true',
        help=("use kl-divergence, shortcut for '--distance {}'"
              .format(KL_DISTANCE_MODULE)))

    args = parser.parse_args()
    
    # Check coherence of inputs 
    if (args.distance == None) and (args.normalized == None): 
        sys.exit("Error : using DTW distance without specifying"
                "normalization parameter")
    if (not args.no_task) and (not args.database):
        sys.exit('Error : path to item file required for task'
                'computation, either specify -database or '
                '--no_task if you already computed the task')

    assert os.path.isdir(args.features) and os.listdir(args.features), (
        'features folder not found or empty')
    item = os.path.join(
            args.database, 'train', args.corpus,
            '{}.item'.format(args.corpus))

    if (args.database): 
        assert os.path.isfile(item),('item file not found'
                'at {}'.format(item))

    if not os.path.exists(args.output):
        try:
            os.makedirs(args.output)
        except:
            sys.sdterr.write(
                'Impossible to create the output directory: {}\n'
                .format(os.path.realpath(args.output)))
            raise

    taskslist = parseConfig(CONFIG_FILE)

    checkIO(taskslist)
    res = {}
    ci = {}
    outfile = os.path.join(args.output, lookup('outputfile', taskslist[0]))
    assert os.access(args.output, os.W_OK), (
        'Impossible to write in the ouput directory, '
        'please check the permissions')

    if args.kl:
        args.distance = KL_DISTANCE_MODULE

    ncpus = args.njobs
    
    if args.verbose:
        verbose=1
    else:
        verbose=0

    # Reduce the .item file to reduce computation time 
    if args.sample: 
       database = sampleitems(item, args.sample, args.output)
    else:
       database = item


    # sys.stderr = Discarder()
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', NaturalNameWarning)
        warnings.simplefilter('ignore', DataTypeWarning)
        # warnings.simplefilter('ignore', ParserWarning)
        for task in taskslist:
            on='phone'
            if task['section'] == 'across_talkers':
                by = ['prev-phone','next-phone']
                across = ['speaker']
            elif task['section'] == 'within_talkers':
                by = ['prev-phone','next-phone','speaker']
                across = []

            (final_score,confusion_mat) = fullrun(
                    task, database,
                    args.features, args.h5,
                    args.no_task,args.corpus,
                    on, across, by, verbose, 
                    args.distance,
                    args.output, args.normalized, ncpus=ncpus)
            print "returned full_score"

            sys.stdout.write(
                    '{}:\t{:.3f}\n'.format(task['section'], final_score))
            res[(task['section'])] = final_score
            confusion_mat.to_csv(os.path.join(args.output,task['section']+'_confusion.csv'))
    try:
        with open(outfile, 'w+') as out:
            out.write('task\tscore\n')
            for key, value in res.iteritems():
                out.write('{}:\t{:.3f}\n'.format(key, value))
        with open(os.path.join(args.output, 'VERSION_' + VERSION), 'w+') as v:
            v.write('')
    except:
        sys.stderr.write('Could not write in the output file {}\n'
                         .format(outfile))
        raise
