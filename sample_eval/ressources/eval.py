import os
import sys
import ABXpy.task
import ABXpy.distances.distances as distances
import ABXpy.distances.metrics.cosine as cosine
import ABXpy.distances.metrics.dtw as dtw
import ABXpy.score as score
import ABXpy.analyze as analyze
import ConfigParser
import argparse
import warnings
import h5py
from tables import DataTypeWarning
from tables import NaturalNameWarning
# from pandas.io.parsers import ParserWarning
import numpy as np
import h5features
import pickle
from ABXpy.misc import any2h5features
import pandas
import ast


version="0.1.0"


if getattr(sys, 'frozen', False):
    # frozen
    rdir = os.path.dirname(sys.executable)
else:
    # unfrozen
    rdir = os.path.dirname(os.path.realpath(__file__))
curdir = os.path.dirname(rdir)


def loadfeats(path):
    aux = np.genfromtxt(path)
    time = aux[:, 0]
    features = aux[:, 1:]
    return {'time': time, 'features': features}


class Discarder(object):
    def __init__(self):
        self.filt = "Exception RuntimeError('Failed to retrieve old handler',) in 'h5py._errors.set_error_handler' ignored"
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
        #self.oldstderr.flush()
        sys.stderr = self.oldstderr


def modified(filepath, mtime):
    return not os.path.exists(filepath) or (mtime > os.path.getmtime(filepath))


def dtw_cosine_distance(x, y):
    return dtw.dtw(x, y, cosine.cosine_distance)


def parseConfig(configfile):
    taskslist = []
    config = ConfigParser.ConfigParser()
    assert os.path.exists(configfile), 'config file not found {}'.format(configfile)
    config.read(configfile)
    assert config.has_section('general'), 'general section missing in config file'
    general_items = dict(config.items('general'))
    sections = [section for section in config.sections() if section != 'general']
    for section in sections:
        task_items = dict(config.items(section))
        task_items['section'] = section
        for item in general_items:
            if item in task_items:
                warnings.warn('general config setting redefined in the task, the task specific one will be used ({}: {})'.format(item, task_items[item]), UserWarning)
            else:
                task_items[item] = general_items[item]
        taskslist.append(task_items)
    return taskslist


def checkIO(taskslist):
    for task_items in taskslist:
        #assert 'on' in task_items, 'missing ON argument for task {}'.format(task['section'])
        #assert 'taskfile' in task_items
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
            return f.attrs.get('done') and (not (f.attrs.get('task') == taskstr))
    except Exception as e:
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
    if task_type=='across':
        # aggregate on context
        groups = df.groupby(['speaker_1','speaker_2','phone_1','phone_2'], as_index=False)
        df = groups['score'].mean()
    elif task_type=='within':

        arr = np.array(map(ast.literal_eval, df['by']))
        df['speaker']  = [e for e, f, g in arr]
        df['context'] = [f for e, f, g in arr]
        #del df['by']
        # aggregate on context
        groups = df.groupby(['speaker','phone_1','phone_2'], as_index=False)
        df = groups['score'].mean()
        
    else:
        raise ValueError('Unknown task type: {0}'.format(task_type))
    #del df['by']
    
    ## aggregate on talkers
    #groups = df.groupby(['context', 'phone_1', 'phone_2'], as_index=False)
    #df = groups['score'].mean()
    ## aggregate on contexts    
    #groups = df.groupby(['phone_1', 'phone_2'], as_index=False) 
    #df = groups['score'].mean()
        
    # aggregate on talker
    groups = df.groupby(['phone_1','phone_2'], as_index=False)
    df = groups['score'].mean()
    average=df.mean()[0]
    return average


def featureshaschanged(feature_folder, feature_file):
    return True


def makedirs(listfiles):
    for f in listfiles:
        pardir = os.path.dirname(f)
        if not os.path.isdir(pardir):
            try:
                os.makedirs(pardir)
            except:
                sys.stderr.write('Could not create directories along path for file {}\n'
                                 .format(os.path.realpath(f)))
                raise


def fullrun(task, feature_folder, h5, file_sizes, corpus, distinction, distance, outputdir, doall=True, ncpus=None):

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
            distancemodule = lookup('distancemodule', task, os.path.join(curdir, 'ressources/distance'))
            distancefunction = lookup('distancefunction', task, 'distance')
            path, mod = os.path.split(distancemodule)
            sys.path.insert(0, path)
            distancefun = getattr(__import__(mod), distancefunction)
    except:
        sys.stderr.write('distance not found\n')
        raise

    distance_file = os.path.join(outputdir, lookup('distancefile', task))
    scorefilename = os.path.join(outputdir, lookup('scorefile', task))
    #taskfilename = os.path.join(curdir, lookup('taskfile', task))
    taskname=os.path.join(lookup('taskdir',task),'{}/{}s_{}_{}.abx'.format(corpus,file_sizes,distinction,lookup('type',task)))
    taskfilename = os.path.join(curdir, taskname)
    analyzefilename = os.path.join(outputdir, lookup('analyzefile', task))
    on = lookup('on', task)
    across = nonesplit(lookup('across', task))
    by = nonesplit(lookup('by', task))
    filters = lookup('filters', task)
    regressors = lookup('regressors', task)
    sampling = lookup('sampling', task)
    if not ncpus:
        ncpus = int(lookup('ncpus', task, 1))

    makedirs([feature_file, distance_file, scorefilename, analyzefilename])

    tasktime = getmtime(taskfilename)
    featuretime = getmtime(feature_file)
    distancetime = getmtime(distance_file)
    scoretime = getmtime(scorefilename)
    analyzetime = getmtime(analyzefilename)
    featfoldertime = max([getmtime(os.path.join(feature_folder, f))
                          for f in os.listdir(feature_folder)])
    # Preprocessing
    if h5==0:
        try:
            print("Preprocessing... Writing the features in h5 format")
            tryremove(feature_file)
            any2h5features.convert(feature_folder, h5_filename=feature_file,
                                   load=loadfeats)
            featuretime = getmtime(feature_file)
            with h5py.File(feature_file) as fh:
                fh.attrs.create('done', True)
        except:
            sys.stderr.write('Error when writing the features from {} to {}\n'
                             'Check the paths availability\n'
                             .format(os.path.realpath(feature_folder),
                                     os.path.realpath(feature_file)))
            #tryremove(feature_file)
            raise
    else:
        feature_file=os.path.join(feature_folder,'{}s_{}.h5f'.format(file_sizes,distinction))

    # computing
    try:
        print("Computing the distances")
        tryremove(distance_file)
        distances.compute_distances(feature_file, '/features/', taskfilename,
                                    distance_file, distancefun, n_cpu=ncpus)

        tryremove(scorefilename)
        print("Computing the scores")
        score.score(taskfilename, distance_file, scorefilename)
        
        tryremove(analyzefilename)
        print("Collapsing the results")
        analyze.analyze(taskfilename, scorefilename, analyzefilename)

        return avg(analyzefilename, task)       
    except:
        sys.stderr.write('An error occured during the computation\n')
        raise
    finally:

        tryremove(distance_file)
        tryremove(scorefilename)
        if h5==0:
            tryremove(feature_file)
        tryremove(analyzefilename)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Full ABX discrimination task')

    # parser.add_argument(
    #     '-c', '--config', default=os.path.join(curdir, 'ressources/sample_eval.cfg'),
    #     help='config file, default to sample_eval.cfg in ressources')
    parser.add_argument(
        'features', help='folder containing the feature to evaluate')
    parser.add_argument(
        'file_sizes',help='''specify which file_sizes you are evaluating. \
                choose between 1 , 10 or 120''')
    parser.add_argument(
        'corpus', help='''corpus you are evaluating.\
                choices are : chinese,english,french''')
    parser.add_argument(
        '-d', '--distance',
        help='distance module to use (distancemodule.distancefunction, '
        'default to dtw cosine distance')
    parser.add_argument(
        '-kl', action='store_true',
        help="use kl-divergence, shortcut for '--distance ressources/distance.kl_divergence'")
    parser.add_argument(
        'output', help='output directory used for intermediate files and results')
    parser.add_argument(
        '-j', help='number of cpus to use')
    parser.add_argument(
        '-h5',type=int,default=0,help='enable if the inputs at already in h5 format. Enabled if h5 !=0')

    args = parser.parse_args()
    config = os.path.join(curdir, 'ressources/eval.cfg')
    taskslist = parseConfig(config)
    speaker_type=['old','new']
    assert os.path.isdir(args.features) and os.listdir(args.features), (
        'features folder not found or empty')
    if not os.path.exists(args.output):
        try:
            os.makedirs(args.output)
        except:
            sys.sdterr.write('Impossible to create the output directory: {}\n'
                             .format(os.path.realpath(args.output)))
            raise        
    checkIO(taskslist)
    res = {}
    outfile = os.path.join(args.output, lookup('outputfile', taskslist[0]))
    assert os.access(args.output, os.W_OK), ('Impossible to write in the ouput directory, '
                                             'please check the permissions')
    if args.kl:
        args.distance = os.path.join(curdir, 'ressources/distance.kl_divergence')
    if args.j:
        try:
            ncpus = int(args.j)
        except:
            sys.stderr.write('invalid number of cpus {}'.format(args.j))
            raise
    else:
        ncpus = None

    sys.stderr = Discarder()
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', NaturalNameWarning)
        warnings.simplefilter('ignore', DataTypeWarning)
        # warnings.simplefilter('ignore', ParserWarning)
        for task in taskslist:
            for distinction in speaker_type:
                final_score = fullrun(task, args.features, args.h5, args.file_sizes, args.corpus,distinction, args.distance, args.output, ncpus=ncpus)
                print "returned full_score"
                sys.stdout.write('{}:\t{:.3f}\n'.format(task['section'], final_score))
                res[(task['section'],distinction)]=final_score

    try:
        with open(outfile, 'w+') as out:
            out.write('task\tscore\n')
            for key, value in res.iteritems():
                out.write('{}:\t{:.3f}\n'.format(key, value))
        with open(os.path.join(args.output, 'VERSION_' + version), 'w+') as version_file:
            version_file.write('')
    except:
        sys.stderr.write('Could not write in the output file {}\n'
                         .format(outfile))
        raise
