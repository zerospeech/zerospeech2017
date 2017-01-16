const char* QN_trn_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_trn.cc,v 1.24 2006/04/06 02:59:10 davidj Exp $";

// Routines for performing MLP trainings.

#include <QN_config.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "QN_types.h"
#include "QN_trn.h"
#include "QN_utils.h"
#include "QN_libc.h"
#include "QN_fltvec.h"
#include "QN_MLPWeightFile_RAP3.h"

// "Hard training" object - trains using labels to indicate targets.

QN_HardSentTrainer::QN_HardSentTrainer(int a_debug, const char* a_dbgname,
				       int a_verbose,
				       QN_MLP* a_mlp,
				       QN_InFtrStream* a_train_ftr_str,
				       QN_InLabStream* a_train_lab_str,
				       QN_InFtrStream* a_cv_ftr_str,
				       QN_InLabStream* a_cv_lab_str,
				       QN_RateSchedule* a_lr_sched,
				       float a_targ_low, float a_targ_high,
				       const char* a_wlog_template,
				       QN_WeightFileType a_wfile_format,
				       const char* a_ckpt_template,
				       QN_WeightFileType a_ckpt_format,
				       unsigned long a_ckpt_secs,
				       size_t a_bunch_size,
				       int a_lastlab_reject,
				       float* a_lrscale)
    : debug(a_debug),
      dbgname(a_dbgname),
      clog(a_debug, "QN_HardSentTrainer", a_dbgname),
      verbose(a_verbose),
      mlp(a_mlp),
      mlp_inps(mlp->size_layer((QN_LayerSelector) 0)),
      mlp_outs(mlp->size_layer((QN_LayerSelector) (mlp->num_layers()-1))),
      train_ftr_str(a_train_ftr_str),
      train_lab_str(a_train_lab_str),
      cv_ftr_str(a_cv_ftr_str),
      cv_lab_str(a_cv_lab_str),
      lr_sched(a_lr_sched),
      targ_low(a_targ_low),
      targ_high(a_targ_high),
      bunch_size(a_bunch_size),
      lastlab_reject(a_lastlab_reject ? 1 : 0),
      inp_buf_size(mlp_inps*bunch_size),
      out_buf_size(mlp_outs*bunch_size),
      targ_buf_size(out_buf_size),
      inp_buf(new float[inp_buf_size]),
      out_buf(new float[out_buf_size]),
      targ_buf(new float[targ_buf_size]),
      lab_buf(new QNUInt32[bunch_size]),
      // Copy the template into a new char array.
      wlog_template(strcpy(new char[strlen(a_wlog_template)+1],
			   a_wlog_template)),
      wfile_format(a_wfile_format),
      ckpt_template(strcpy(new char[strlen(a_ckpt_template)+1],
			   a_ckpt_template)),
      ckpt_format(a_ckpt_format),
      ckpt_secs(a_ckpt_secs),
      last_ckpt_time(time(NULL)),
      pid(getpid())
{
// Perform some checks of the input data.
    assert(bunch_size!=0);

    size_t i;
    // Copy across the lrscale vals
    if (a_lrscale!=NULL)
    {
	for (i=0; i<mlp->num_layers()-1; i++)
	    lrscale[i] = a_lrscale[i];
    }
    else
    {
	for (i=0; i<mlp->num_layers()-1; i++)
	    lrscale[i] = 1.0f;
    }
    // Check the input streams.
    if (train_ftr_str->num_ftrs()!=mlp_inps)
    {
	clog.error("The training feature stream has %lu features, the "
		   "MLP has %lu inputs.",
		   (unsigned long) train_ftr_str->num_ftrs(),
		   (unsigned long) mlp_inps);
    }
    if (cv_ftr_str->num_ftrs()!=mlp_inps)
    {
	clog.error("The CV feature stream has %lu features, the "
		   "MLP has %lu inputs.",
		   (unsigned long) cv_ftr_str->num_ftrs(),
		   (unsigned long) mlp_inps);
    }
    if (train_lab_str->num_labs()!=1)
    {
	clog.error("The train label stream has %lu labels per frame but we "
		   "can only use 1.",
		   (unsigned long) train_lab_str->num_labs());
    }
    if (cv_lab_str->num_labs()!=1)
    {
	clog.error("The CV label stream has %lu labels per frame but we "
		   "can only use 1.",
		   (unsigned long) cv_lab_str->num_labs());
    }

    // Set up the weight logging stuff.
    if (QN_logfile_template_check(wlog_template)!=QN_OK)
    {
	clog.error("Invalid weight log file "
		 "template \'%s\'.", wlog_template);

    }
    // Set up the weight checkpointing stuff.
    if (QN_logfile_template_check(ckpt_template)!=QN_OK)
    {
	clog.error("Invalid ckpt file template \'%s\'.", ckpt_template);

    }
}

QN_HardSentTrainer::~QN_HardSentTrainer()
{
    delete[] ckpt_template;
    delete[] wlog_template;
    delete[] inp_buf;
    delete[] out_buf;
    delete[] targ_buf;
    delete[] lab_buf;
}

void
QN_HardSentTrainer::train()
{
    double run_start_time;	// Time we started.
    double run_stop_time;	// Time we stopped.
    char timebuf[QN_TIMESTR_BUFLEN]; // Somewhere to put the current time.
    float percent_correct;	// Percentage correct on current test.
    float last_cv_error;	// Percentage error from last cross validation.
    size_t best_cv_epoch;	// Epoch of best cross validation error.
    size_t best_train_epoch;	// Epoch of best training error.
    float best_cv_error;	// Best CV error percentage.
    float best_train_error;	// Best train error percentage.
    char last_weightlog_filename[MAXPATHLEN];

    // Startup log messages.
    QN_timestr(timebuf, sizeof(timebuf));
    QN_OUTPUT("Training run start: %s.", timebuf);
    QN_OUTPUT("** ** ** ** ** ** ** ** ** ** ** ** ** **");
    run_start_time = QN_time();

    // Pre-training cross validation.
    QN_timestr(timebuf, sizeof(timebuf));
    QN_OUTPUT("Pre-run cross validation started: %s.", timebuf);
    percent_correct = cv_epoch();
    QN_timestr(timebuf, sizeof(timebuf));
    if (verbose)
	QN_OUTPUT("Pre-run cross validation finished: %s.", timebuf);

    // Note: to prevent the initial weights being saved as the best weights,
    // we assume the cross validation had abysmal results.  Even if the
    // training makes things worse, the change might be useful and we do not
    // want the resulting weights to be from the initialization file.
    last_cv_error = 100.0f;
    best_cv_error = 100.0f;
    best_train_error = 100.0f;
    best_cv_epoch = 0;
    best_train_epoch = 0;
    learn_rate = lr_sched->get_rate();
    epoch = 1;			// Epochs are numbered starting from 1.

    while (learn_rate!=0.0f)	// Iterate over all epochs.
    {
	float train_error;	// Percentage error on current training.
	float cv_error;		// Percentage error on current CV.
	
	// Epoch startup status.
	QN_OUTPUT("** ** ** ** ** ** ** ** ** ** ** ** ** **");
	QN_OUTPUT("New epoch: epoch %i, learn rate %f.", epoch, learn_rate);
	QN_timestr(timebuf, sizeof(timebuf));
	QN_OUTPUT("Epoch started: %s.", timebuf);

	// Training phase.
	set_learnrate();	// Set the learning rate 
	train_ftr_str->rewind();
	train_lab_str->rewind();
	percent_correct = train_epoch();
	train_error = 100.0f - percent_correct;
	if (train_error < best_train_error)
	{
	    best_train_error = train_error;
	    best_train_epoch = epoch;
	}
	QN_timestr(timebuf, sizeof(timebuf));
	if (verbose)
	    QN_OUTPUT("Training finished/CV started: %s.", timebuf);

	// Cross validation phase.
	cv_ftr_str->rewind();
	cv_lab_str->rewind();
	percent_correct = cv_epoch();
	cv_error = 100.0f - percent_correct;

	// Keeping track of how well we are doing.
	if (cv_error > last_cv_error)
	{
	    QN_OUTPUT("Weight log: Restoring previous weights from "
	    "`%s\'.", last_weightlog_filename);
	    QN_readwrite_weights(debug, dbgname, *mlp,
				 last_weightlog_filename, wfile_format,
				 QN_READ);
	}
	else
	{
	    int ec;		// Error code.

	    ec = QN_logfile_template_map(wlog_template,
					 last_weightlog_filename, MAXPATHLEN,
					 epoch, pid);
	    if (ec!=QN_OK)
	    {
		clog.error("failed to build weight log file name from "
			   "template \'%s\'.", wlog_template);
	    }
	    QN_OUTPUT("Weight log: Saving weights to `%s\'.",
		      last_weightlog_filename);
	    QN_readwrite_weights(debug,dbgname, *mlp,
				 last_weightlog_filename, wfile_format, QN_WRITE);
	    last_cv_error = cv_error;
	    best_cv_error = cv_error;
	    best_cv_epoch = epoch;
	}

	// Epoch end status.
	QN_timestr(timebuf, sizeof(timebuf));
	if (verbose)
	    QN_OUTPUT("Epoch finished: %s.", timebuf);

	// On to next epoch.
	learn_rate = lr_sched->next_rate(cv_error);
	epoch++;
    }

    // Wind down log messages.
    QN_OUTPUT("** ** ** ** ** ** ** ** ** ** ** ** ** **");
    QN_OUTPUT("Best CV accuracy: %.2f%% correct in epoch %i.",
	    100.0 - best_cv_error, (int) best_cv_epoch);
    QN_OUTPUT("Best train accuracy: %.2f%% correct in epoch %i.",
	    100.0 - best_train_error, (int) best_train_epoch);
    run_stop_time = QN_time();
    QN_timestr(timebuf, sizeof(timebuf));
    QN_OUTPUT("Training run stop: %s.", timebuf);
    double total_time = run_stop_time - run_start_time;
    int hours = ((int) total_time) / 3600;
    int mins = (((int) total_time) / 60) % 60;
    int secs = ((int) total_time) % 60;
    QN_OUTPUT("Training time: %.2f secs (%i hours, %i mins, %i secs).",
	      total_time, hours, mins, secs);
    QN_OUTPUT("** ** ** ** ** ** ** ** ** ** ** ** ** **");
}

// Note - cross validation is less demanding on the facilities that
// must be provided by the stream.
double
QN_HardSentTrainer::cv_epoch()
{
    size_t ftr_count;		// Count of feature frames read.
    size_t lab_count;		// Count of label frames read.
    size_t total_frames = 0;	// Total frames read this phase.
    size_t correct_frames = 0;	// Number of correct frames this phase.
    size_t reject_frames = 0;	// Numer of frames we reject.
    size_t current_segno;	// Current segment number.
    double start_secs;		// Exact time we started.
    double stop_secs;		// Exact time we stopped.
    size_t i;			// Local counter.

    current_segno = 0;
    ftr_count = 0;		// Pretend that previous read hit end of seg.
    start_secs = QN_time();
    
    // Iterate over all input segments.
    // Note that, at this stage, an input segment is _not_ typically a
    // sentence, but a buffer full of sentences (known as a "fill" in old
    // QuickNet code).
    while (1)
    {
	if (ftr_count<bunch_size) // Check if at end of segment.
	{
	    QN_SegID ftr_segid;	// Segment ID from input stream.
	    QN_SegID lab_segid;	// Segment ID from label stream.

	    ftr_segid = cv_ftr_str->nextseg();
	    lab_segid = cv_lab_str->nextseg();
	    assert(ftr_segid==lab_segid);
	    if (ftr_segid==QN_SEGID_BAD)
		break;
	    else
	    {
		current_segno++;
	    }
	}

	// Get the data to pass on to the net.
	ftr_count = cv_ftr_str->read_ftrs(bunch_size, inp_buf);
	lab_count = cv_lab_str->read_labs(bunch_size, lab_buf);
	if (ftr_count!=lab_count)
	{
	    clog.error("Feature and label streams have different segment "
		       "lengths in cross validation.");
	}

	// Do the forward pass.
	mlp->forward(ftr_count, inp_buf, out_buf);
	
	// Analyze the output of the net.
	float* out_buf_ptr = out_buf; // Current output frame.
	QNUInt32* lab_buf_ptr = lab_buf; // Current label.
	QNUInt32 net_label;		// Label chosen by the net.
	QNUInt32 cv_label;		// Label in cv stream.
	for (i=0; i<ftr_count; i++)
	{
	    net_label = qn_imax_vf_u(mlp_outs, out_buf_ptr);
	    cv_label = *lab_buf_ptr;
	    if (cv_label >= (mlp_outs + lastlab_reject))
	    {
		clog.error("Label value of %lu in CV stream is "
			   "larger than the number of output units.",
			   (unsigned long) cv_label);
	    }
	    if (lastlab_reject && cv_label==mlp_outs)
		reject_frames++;
	    else if (net_label==cv_label)
		correct_frames++;
	    out_buf_ptr += mlp_outs;
	    lab_buf_ptr++;
	}
	total_frames += ftr_count;
    }
    stop_secs = QN_time();
    double total_secs = stop_secs - start_secs;
    size_t unreject_frames = total_frames - reject_frames;
    double percent = 100.0 * (double) correct_frames
	/ (double) unreject_frames; 

    QN_OUTPUT("CV speed: %.2f MCPS, %.1f presentations/sec.",
	      QN_secs_to_MCPS(stop_secs-start_secs, total_frames, *mlp),
	      (double) total_frames / total_secs);
    QN_OUTPUT("CV accuracy:  %lu right out of %lu, %.2f%% correct.",
	      (unsigned long) correct_frames, (unsigned long) unreject_frames,
	      percent);
    if (lastlab_reject)
    {
	double percent_reject = 100.0
		* (double)reject_frames / (double) total_frames;
	QN_OUTPUT("CV reject frames: %lu of total %lu frames rejected, "
		  "%.2f%% rejected.",
		  (unsigned long) reject_frames,
		  (unsigned long) total_frames,
		  percent_reject);
    }

    return percent; 
}

double
QN_HardSentTrainer::train_epoch()
{
    size_t ftr_count;		// Count of feature frames read.
    size_t lab_count;		// Count of label frames read.
    size_t trn_count;		// Count of frames to train on.
    size_t total_frames = 0;	// Total frames read this phase.
    size_t seg_frames = 0;      // Number of frames in this segment.
    size_t reject_frames = 0;	// Total number of rejects this phase.
    size_t unreject_frames;	// Number of not rejected frames.
    size_t correct_frames = 0;	// Number of correct frames this phase.
    size_t total_segs;		// Number of segments in streams.
    size_t current_segno;	// Current segment number.
    time_t current_time;	// Current time.
    double start_secs;		// Exact time we started.
    double stop_secs;		// Exact time we stopped.
    double total_secs;		// Total time.
    double percent;
    size_t i;			// Local counter.

    assert(bunch_size!=0);

    total_segs = train_ftr_str->num_segs();
    current_segno = 0;
    ftr_count = 0;		// Pretend that previous read hit end of seg.
    start_secs = QN_time();
    
    // Iterate over all input segments.
    // Note that, at this stage, an input segment is _not_ typically a
    // sentence, but a buffer full of sentences (known as a "fill" in old
    // QuickNet code).
    while (1)
    {
	if (ftr_count<bunch_size) // Check if at end of segment.
	{
	    QN_SegID ftr_segid;	// Segment ID from input stream.
	    QN_SegID lab_segid;	// Segment ID from label stream.
	    
	    // update the learning rate, for those schedules that care
	    float nlearn_rate=lr_sched->trained_on_nsamples(seg_frames);
	    if (nlearn_rate != learn_rate) {
	      learn_rate=nlearn_rate;
	      set_learnrate();  // Pass the info along to the MLP
	      if (verbose) 
		QN_OUTPUT("learning rate set to %.6f after %d samples read",learn_rate,seg_frames);
	    }
	    seg_frames=0;
	    
	    ftr_segid = train_ftr_str->nextseg();
	    lab_segid = train_lab_str->nextseg();
	    assert(ftr_segid==lab_segid);
	    if (ftr_segid==QN_SEGID_BAD)
		break;
	    else
	    {
		if (verbose)
		{
		    QN_OUTPUT("Starting chunk %lu of %lu containing "
			      "%lu frames...",
			      (unsigned long) (current_segno+1),
			      (unsigned long) total_segs,
			      train_ftr_str->num_frames(current_segno));
		}
		current_segno++;
	    }
	}

	// Get the data to pass on to the net.
	ftr_count = train_ftr_str->read_ftrs(bunch_size, inp_buf);
	lab_count = train_lab_str->read_labs(bunch_size, lab_buf);
	if (ftr_count!=lab_count)
	{
	    clog.error("Feature and label streams have different segment "
		       "lengths in cross validation.");
	}

	// Check that the label stream is good and build up the target vector.
	qn_copy_f_vf(ftr_count*mlp_outs, targ_low, targ_buf);
	float* targ_buf_ptr = targ_buf;	// Target values put here
	QNUInt32* lab_buf_ptr = lab_buf; // Labels taken from here
	QNUInt32 lab;		// Label to train to
	if (lastlab_reject)
	{
	    // Set up the input and target vectors taking account of rejects
	    // Note that we compress the input features in place.
	    float* inp_buf_ptr = inp_buf; // Get all frames from here
	    float* unrej_buf_ptr = inp_buf; // Put unrejected frames here

	    trn_count = 0;
	    for (i=0; i<lab_count; i++)
	    {

		lab = *lab_buf_ptr;
		if (lab>mlp_outs)
		{
		    clog.error("Label in train stream is larger than the "
			       "number of output units.");
		}
		else if (lab==mlp_outs)
		{
		    // Rejected frame.
		    reject_frames++;
		}
		else
		{
		    // Unrejected frame.
		    trn_count++;
		    targ_buf_ptr[lab] = targ_high;
		    qn_copy_vf_vf(mlp_inps, inp_buf_ptr, unrej_buf_ptr);
		    targ_buf_ptr += mlp_outs;
		    unrej_buf_ptr += mlp_inps;
		}
		inp_buf_ptr+= mlp_inps;
		lab_buf_ptr++;
	    }
	}
	else
	{
	    // Here we set up the target vector without reject labels
	    for (i=0; i<lab_count; i++)
	    {
		lab = *lab_buf_ptr;
		if (lab>=mlp_outs)
		{
		    clog.error("Label in train stream is larger than the "
			       "number of output units.");
		}
		targ_buf_ptr[lab] = targ_high;
		lab_buf_ptr++;
		targ_buf_ptr += mlp_outs;
	    }
	    trn_count = lab_count;
	}

	// Do the training.
	mlp->train(trn_count, inp_buf, targ_buf, out_buf);

	// Analyze the output of the net.
	float* out_buf_ptr = out_buf; // Current output frame.
	lab_buf_ptr = lab_buf;	// Current label.
	QNUInt32 net_label;		// Label chosen by the net.
	for (i=0; i<trn_count; i++)
	{
	    net_label = qn_imax_vf_u(mlp_outs, out_buf_ptr);
	    while (*lab_buf_ptr==mlp_outs)
		lab_buf_ptr++; // skip reject labels
	    if (net_label==*lab_buf_ptr)
		correct_frames++;
	    out_buf_ptr += mlp_outs;
	    lab_buf_ptr++;
	}
	total_frames += ftr_count;
	seg_frames += ftr_count;

	// Checkpoint if necessary.
	current_time = time(NULL);
	if (ckpt_secs!=0 && (current_time > (last_ckpt_time + ckpt_secs)))
	{
	    last_ckpt_time = current_time;
	    checkpoint_weights();
	}
    }


    stop_secs = QN_time();
    total_secs = stop_secs - start_secs;
    unreject_frames = total_frames - reject_frames;
    percent = 100.0 * (double) correct_frames / (double) unreject_frames;

    QN_OUTPUT("Train speed: %.2f MCUPS, %.1f presentations/sec.",
	      QN_secs_to_MCPS(stop_secs-start_secs, unreject_frames, *mlp),
	      (double) unreject_frames / total_secs);
    QN_OUTPUT("Train accuracy:  %lu right out of %lu, %.2f%% correct.",
	      (unsigned long) correct_frames, (unsigned long) unreject_frames,
	      percent);
    if (lastlab_reject)
    {
	double percent_reject;

	percent_reject = 100.0 * (double)reject_frames / (double)total_frames;
	QN_OUTPUT("Train reject frames: %lu of total %lu frames rejected, "
		  "%.2f%% rejected.",
		  (unsigned long) reject_frames,
		  (unsigned long) total_frames,
		  percent_reject);
    }

    return percent; 
}

// Set the learning rate for the net according to the learn_rate variable.
void
QN_HardSentTrainer::set_learnrate()
{
    size_t i;
    float scale;		// Learnrate scaler.

    for (i=0; i<mlp->num_sections(); i++)
    {
	scale = lrscale[i/2];
	mlp->set_learnrate((QN_SectionSelector) i, learn_rate*scale);
    }
}

void
QN_HardSentTrainer::checkpoint_weights()
{
    int ec;
    char ckpt_filename[MAXPATHLEN]; // Checkpoint filename.
    
    ec = QN_logfile_template_map(ckpt_template,
				 ckpt_filename, MAXPATHLEN,
				 epoch, pid);
    if (ec!=QN_OK)
    {
	clog.error("failed to build ckpt weight file name from "
		   "template \'%s\'.", ckpt_template);
    }
    QN_OUTPUT("Checkpoint: Saving weights to `%s\'.",
	      ckpt_filename);
    QN_readwrite_weights(debug,dbgname, *mlp,
			 ckpt_filename, ckpt_format, QN_WRITE);

}


////////////////////////////////////////////////////////////////
// "Soft training" object - trains using continuous targets.

QN_SoftSentTrainer::QN_SoftSentTrainer(int a_debug, const char* a_dbgname,
				       int a_verbose,
				       QN_MLP* a_mlp,
				       QN_InFtrStream* a_train_ftr_str,
				       QN_InFtrStream* a_train_targ_str,
				       QN_InFtrStream* a_cv_ftr_str,
				       QN_InFtrStream* a_cv_targ_str,
				       QN_RateSchedule* a_lr_sched,
				       float a_targ_low, float a_targ_high,
				       const char* a_wlog_template,
				       QN_WeightFileType a_wfile_format,
				       const char* a_ckpt_template,
				       QN_WeightFileType a_ckpt_format,
				       unsigned long a_ckpt_secs,
				       size_t a_bunch_size,
				       float* a_lrscale)
    : clog(a_debug, "QN_SoftSentTrainer", a_dbgname),
      verbose(a_verbose),
      mlp(a_mlp),
      mlp_inps(mlp->size_layer((QN_LayerSelector) 0)),
      mlp_outs(mlp->size_layer((QN_LayerSelector) (mlp->num_layers()-1))),
      train_ftr_str(a_train_ftr_str),
      train_targ_str(a_train_targ_str),
      cv_ftr_str(a_cv_ftr_str),
      cv_targ_str(a_cv_targ_str),
      lr_sched(a_lr_sched),
      targ_low(a_targ_low),
      targ_high(a_targ_high),
      bunch_size(a_bunch_size),
      inp_buf_size(mlp_inps*bunch_size),
      out_buf_size(mlp_outs*bunch_size),
      targ_buf_size(out_buf_size),
      inp_buf(new float[inp_buf_size]),
      out_buf(new float[out_buf_size]),
      targ_buf(new float[targ_buf_size]),
      // Copy the template into a new char array.
      wlog_template(strcpy(new char[strlen(a_wlog_template)+1],
			   a_wlog_template)),
      wfile_format(a_wfile_format),
      ckpt_template(strcpy(new char[strlen(a_ckpt_template)+1],
			   a_ckpt_template)),
      ckpt_format(a_ckpt_format),
      ckpt_secs(a_ckpt_secs),
      last_ckpt_time(time(NULL)),
      pid(getpid())
{
// Perform some checks of the input data.
    assert(bunch_size!=0);

    size_t i;
    // Copy across the lrscale vals
    if (a_lrscale!=NULL)
    {
	for (i=0; i<mlp->num_layers()-1; i++)
	    lrscale[i] = a_lrscale[i];
    }
    else
    {
	for (i=0; i<mlp->num_layers()-1; i++)
	    lrscale[i] = 1.0f;
    }
    // Check the input streams.
    if (train_ftr_str->num_ftrs()!=mlp_inps)
    {
	clog.error("The training feature stream has %lu features, the "
		   "MLP has %lu inputs.",
		   (unsigned long) train_ftr_str->num_ftrs(),
		   (unsigned long) mlp_inps);
    }
    if (cv_ftr_str->num_ftrs()!=mlp_inps)
    {
	clog.error("The CV feature stream has %lu features, the "
		   "MLP has %lu inputs.",
		   (unsigned long) cv_ftr_str->num_ftrs(),
		   (unsigned long) mlp_inps);
    }
    if (train_targ_str->num_ftrs()!=mlp_outs)
    {
	clog.error("The training target stream has %lu features, the "
		   "MLP has %lu outputs.",
		   (unsigned long) train_targ_str->num_ftrs(),
		   (unsigned long) mlp_outs);
    }
    if (cv_targ_str->num_ftrs()!=mlp_outs)
    {
	clog.error("The CV target stream has %lu features, the "
		   "MLP has %lu outputs.",
		   (unsigned long) cv_targ_str->num_ftrs(),
		   (unsigned long) mlp_outs);
    }

    // Set up the weight logging stuff.
    if (QN_logfile_template_check(wlog_template)!=QN_OK)
    {
	clog.error("Invalid weight log file "
		 "template \'%s\'.", wlog_template);

    }

    // Set up the weight checkpointing stuff.
    if (QN_logfile_template_check(ckpt_template)!=QN_OK)
    {
	clog.error("Invalid ckpt file template \'%s\'.", ckpt_template);

    }
}

QN_SoftSentTrainer::~QN_SoftSentTrainer()
{
    delete[] ckpt_template;
    delete[] wlog_template;
    delete[] inp_buf;
    delete[] out_buf;
    delete[] targ_buf;
}

void
QN_SoftSentTrainer::train()
{
    double run_start_time;	// Time we started.
    double run_stop_time;	// Time we stopped.
    char timebuf[QN_TIMESTR_BUFLEN]; // Somewhere to put the current time.
    float percent_correct;	// Percentage correct on current test.
    float last_cv_error;	// Percentage error from last cross validation.
    size_t best_cv_epoch;	// Epoch of best cross validation error.
    size_t best_train_epoch;	// Epoch of best training error.
    float best_cv_error;	// Best CV error percentage.
    float best_train_error;	// Best train error percentage.
    char last_weightlog_filename[MAXPATHLEN];

    // Startup log messages.
    QN_timestr(timebuf, sizeof(timebuf));
    QN_OUTPUT("Training run start: %s.", timebuf);
    QN_OUTPUT("** ** ** ** ** ** ** ** ** ** ** ** ** **");
    run_start_time = QN_time();

    // Pre-training cross validation.
    QN_timestr(timebuf, sizeof(timebuf));
    QN_OUTPUT("Pre-run cross validation started: %s.", timebuf);
    percent_correct = cv_epoch();
    QN_timestr(timebuf, sizeof(timebuf));
    if (verbose)
	QN_OUTPUT("Pre-run cross validation finished: %s.", timebuf);

    // Note: to prevent the initial weights being saved as the best weights,
    // we assume the cross validation had abysmal results.  Even if the
    // training makes things worse, the change might be useful and we do not
    // want the resulting weights to be from the initialization file.
    last_cv_error = 100.0f;
    best_cv_error = 100.0f;
    best_train_error = 100.0f;
    best_cv_epoch = 0;
    best_train_epoch = 0;
    learn_rate = lr_sched->get_rate();
    epoch = 1;			// Epochs are numbered starting from 1.

    while (learn_rate!=0.0f)	// Iterate over all epochs.
    {
	float train_error;	// Percentage error on current training.
	float cv_error;		// Percentage error on current CV.
	
	// Epoch startup status.
	QN_OUTPUT("** ** ** ** ** ** ** ** ** ** ** ** ** **");
	QN_OUTPUT("New epoch: epoch %i, learn rate %f.", epoch, learn_rate);
	QN_timestr(timebuf, sizeof(timebuf));
	QN_OUTPUT("Epoch started: %s.", timebuf);

	// Training phase.
	set_learnrate();	// Set the learning rate 
	train_ftr_str->rewind();
	train_targ_str->rewind();
	percent_correct = train_epoch();
	train_error = 100.0f - percent_correct;
	if (train_error < best_train_error)
	{
	    best_train_error = train_error;
	    best_train_epoch = epoch;
	}
	QN_timestr(timebuf, sizeof(timebuf));
	if (verbose)
	    QN_OUTPUT("Training finished/CV started: %s.", timebuf);

	// Cross validation phase.
	cv_ftr_str->rewind();
	cv_targ_str->rewind();
	percent_correct = cv_epoch();
	cv_error = 100.0f - percent_correct;

	// Keeping track of how well we are doing.
	if (cv_error > last_cv_error)
	{
	    QN_OUTPUT("Weight log: Restoring previous weights from "
	    "`%s\'.", last_weightlog_filename);
	    QN_readwrite_weights(debug, dbgname, 
				 *mlp, last_weightlog_filename,
				 wfile_format, QN_READ);
	}
	else
	{
	    int ec;		// Error code.

	    ec = QN_logfile_template_map(wlog_template,
					 last_weightlog_filename, MAXPATHLEN,
					 epoch, pid);
	    if (ec!=QN_OK)
	    {
		clog.error("failed to build weight log file name from "
			   "template \'%s\'.", wlog_template);
	    }
	    QN_OUTPUT("Weight log: Saving weights to `%s\'.",
		      last_weightlog_filename);
	    QN_readwrite_weights(debug, dbgname, *mlp, last_weightlog_filename,
				 wfile_format, QN_WRITE);
	    last_cv_error = cv_error;
	    best_cv_error = cv_error;
	    best_cv_epoch = epoch;
	}

	// Epoch end status.
	QN_timestr(timebuf, sizeof(timebuf));
	if (verbose)
	    QN_OUTPUT("Epoch finished: %s.", timebuf);

	// On to next epoch.
	learn_rate = lr_sched->next_rate(cv_error);
	epoch++;
    }

    // Wind down log messages.
    QN_OUTPUT("** ** ** ** ** ** ** ** ** ** ** ** ** **");
    QN_OUTPUT("Best CV accuracy: %.2f%% correct in epoch %i.",
	    100.0 - best_cv_error, (int) best_cv_epoch);
    QN_OUTPUT("Best train accuracy: %.2f%% correct in epoch %i.",
	    100.0 - best_train_error, (int) best_train_epoch);
    run_stop_time = QN_time();
    QN_timestr(timebuf, sizeof(timebuf));
    QN_OUTPUT("Training run stop: %s.", timebuf);
    double total_time = run_stop_time - run_start_time;
    int hours = ((int) total_time) / 3600;
    int mins = (((int) total_time) / 60) % 60;
    int secs = ((int) total_time) % 60;
    QN_OUTPUT("Training time: %.2f secs (%i hours, %i mins, %i secs).",
	      total_time, hours, mins, secs);
    QN_OUTPUT("** ** ** ** ** ** ** ** ** ** ** ** ** **");
}

// Note - cross validation is less demanding on the facilities that
// must be provided by the stream.
double
QN_SoftSentTrainer::cv_epoch()
{
    size_t ftr_count;		// Count of feature frames read.
    size_t targ_count;		// Count of target frames read.
    size_t total_frames = 0;	// Total frames read this phase.
    size_t correct_frames = 0;	// Number of correct frames this phase.
    size_t current_segno;	// Current segment number.
    double start_secs;		// Exact time we started.
    double stop_secs;		// Exact time we stopped.
    size_t i;			// Local counter.

    current_segno = 0;
    ftr_count = 0;		// Pretend that previous read hit end of seg.
    start_secs = QN_time();
    
    // Iterate over all input segments.
    // Note that, at this stage, an input segment is _not_ typically a
    // sentence, but a buffer full of sentences (known as a "fill" in old
    // QuickNet code).
    while (1)
    {
	if (ftr_count<bunch_size) // Check if at end of segment.
	{
	    QN_SegID ftr_segid;	// Segment ID from input stream.
	    QN_SegID targ_segid; // Segment ID from target stream.

	    ftr_segid = cv_ftr_str->nextseg();
	    targ_segid = cv_targ_str->nextseg();
	    assert(ftr_segid==targ_segid);
	    if (ftr_segid==QN_SEGID_BAD)
		break;
	    else
	    {
		current_segno++;
	    }
	}

	// Get the data to pass on to the net.
	ftr_count = cv_ftr_str->read_ftrs(bunch_size, inp_buf);
	targ_count = cv_targ_str->read_ftrs(bunch_size, targ_buf);
	if (ftr_count!=targ_count)
	{
	    clog.error("Feature and label streams have different segment "
		       "lengths in cross validation.");
	}
	// Do the forward pass.
	mlp->forward(ftr_count, inp_buf, out_buf);
	
	// Analyze the output of the net.
	float* out_buf_ptr = out_buf; // Current output frame.
	float* targ_buf_ptr = targ_buf; // Current target vector.
	QNUInt32 net_label;		// Label chosen by the net.
	QNUInt32 targ_label;		// Label given by targets.
	for (i=0; i<ftr_count; i++)
	{
	    net_label = qn_imax_vf_u(mlp_outs, out_buf_ptr);
	    targ_label = qn_imax_vf_u(mlp_outs, targ_buf_ptr);
	    if (net_label==targ_label)
		correct_frames++;
	    out_buf_ptr += mlp_outs;
	    targ_buf_ptr += mlp_outs;
	}
	total_frames += ftr_count;
    }
    stop_secs = QN_time();
    double total_secs = stop_secs - start_secs;
    double percent = 100.0 * (double) correct_frames / (double) total_frames;

    QN_OUTPUT("CV speed: %.2f MCPS, %.1f presentations/sec.",
	      QN_secs_to_MCPS(stop_secs-start_secs, total_frames, *mlp),
	      (double) total_frames / total_secs);
    QN_OUTPUT("CV accuracy:  %lu right out of %lu, %.2f%% correct.",
	      (unsigned long) correct_frames, (unsigned long) total_frames,
	      percent);

    return percent; 
}

double
QN_SoftSentTrainer::train_epoch()
{
    size_t ftr_count;		// Count of feature frames read.
    size_t targ_count;		// Count of target frames read.
    size_t total_frames = 0;	// Total frames read this phase.
    size_t seg_frames = 0;      // Number of frames in this segment.
    size_t correct_frames = 0;	// Number of correct frames this phase.
    size_t total_segs;		// Number of segments in streams.
    size_t current_segno;	// Current segment number.
    time_t current_time;	// Current time.
    double start_secs;		// Exact time we started.
    double stop_secs;		// Exact time we stopped.
    double total_secs;		// Total time.
    double percent;
    size_t i;			// Local counter.

    assert(bunch_size!=0);

    total_segs = train_ftr_str->num_segs();
    current_segno = 0;
    ftr_count = 0;		// Pretend that previous read hit end of seg.
    start_secs = QN_time();
    
    // Iterate over all input segments.
    // Note that, at this stage, an input segment is _not_ typically a
    // sentence, but a buffer full of sentences (known as a "fill" in old
    // QuickNet code).
    while (1)
    {
	if (ftr_count<bunch_size) // Check if at end of segment.
	{
	    QN_SegID ftr_segid;	// Segment ID from input stream.
	    QN_SegID targ_segid; // Segment ID from target stream.

	    // update the learning rate, for those schedules that care
	    float nlearn_rate=lr_sched->trained_on_nsamples(seg_frames);
	    if (nlearn_rate != learn_rate) {
	      learn_rate=nlearn_rate;
	      set_learnrate();  // Pass the info along to the MLP
	      if (verbose) 
		QN_OUTPUT("learning rate set to %.6f after %d samples read",learn_rate,seg_frames);
	    }
	    seg_frames=0;

	    ftr_segid = train_ftr_str->nextseg();
	    targ_segid = train_targ_str->nextseg();
	    assert(ftr_segid==targ_segid);
	    if (ftr_segid==QN_SEGID_BAD)
		break;
	    else
	    {
		if (verbose)
		{
		    QN_OUTPUT("Starting chunk %lu of %lu containing "
			      "%lu frames...",
			      (unsigned long) (current_segno+1),
			      (unsigned long) total_segs,
			      train_ftr_str->num_frames(current_segno));
		}
		current_segno++;
	    }
	}

	// Get the data to pass on to the net.
	ftr_count = train_ftr_str->read_ftrs(bunch_size, inp_buf);
	targ_count = train_targ_str->read_ftrs(bunch_size, targ_buf);
	if (ftr_count!=targ_count)
	{
	    clog.error("Feature and label streams have different "
		       "lengths in cross validation.");
	}

	// Do the training.
	mlp->train(ftr_count, inp_buf, targ_buf, out_buf);

	// Analyze the output of the net.
	float* out_buf_ptr = out_buf; // Current output frame.
	float* targ_buf_ptr = targ_buf; // Current target vector.
	QNUInt32 net_label;		// Label chosen by the net.
	QNUInt32 targ_label;		// Label from file.
	for (i=0; i<ftr_count; i++)
	{
	    net_label = qn_imax_vf_u(mlp_outs, out_buf_ptr);
	    targ_label = qn_imax_vf_u(mlp_outs, targ_buf_ptr);
	    if (net_label==targ_label)
		correct_frames++;
	    out_buf_ptr += mlp_outs;
	    targ_buf_ptr += mlp_outs;
	}
	total_frames += ftr_count;
	seg_frames += ftr_count;

	current_time = time(NULL);
	if (ckpt_secs!=0 && (current_time > (last_ckpt_time + ckpt_secs)))
	{
	    last_ckpt_time = current_time;
	    checkpoint_weights();
	}
    }
    stop_secs = QN_time();
    total_secs = stop_secs - start_secs;
    percent = 100.0 * (double) correct_frames / (double) total_frames;

    QN_OUTPUT("Train speed: %.2f MCUPS, %.1f presentations/sec.",
	      QN_secs_to_MCPS(stop_secs-start_secs, total_frames, *mlp),
	      (double) total_frames / total_secs);
    QN_OUTPUT("Train accuracy:  %lu right out of %lu, %.2f%% correct.",
	      (unsigned long) correct_frames, (unsigned long) total_frames,
	      percent);

    return percent; 
}

// Set the learning rate for the net according to the learn_rate variable.
void
QN_SoftSentTrainer::set_learnrate()
{
    size_t i;
    float scale;		// Learnrate scaler.

    for (i=0; i<mlp->num_sections(); i++)
    {
	scale = lrscale[i/2];
	mlp->set_learnrate((QN_SectionSelector) i, learn_rate*scale);
    }
}



void
QN_SoftSentTrainer::checkpoint_weights()
{
    int ec;
    char ckpt_filename[MAXPATHLEN]; // Checkpoint filename.
    
    ec = QN_logfile_template_map(ckpt_template,
				 ckpt_filename, MAXPATHLEN,
				 epoch, pid);
    if (ec!=QN_OK)
    {
	clog.error("failed to build ckpt weight file name from "
		   "template \'%s\'.", ckpt_template);
    }
    QN_OUTPUT("Checkpoint: Saving weights to `%s\'.",
	      ckpt_filename);
    QN_readwrite_weights(debug,dbgname, *mlp,
			 ckpt_filename, ckpt_format, QN_WRITE);

}

