// $Header: /u/drspeech/repos/quicknet2/QN_trn.h,v 1.14 2006/04/06 02:59:11 davidj Exp $

#ifndef QN_trn_h_INCLUDED
#define QN_trn_h_INCLUDED


#include <QN_config.h>
#include <assert.h>
#include <stddef.h>
#ifdef QN_HAVE_LIMITS_H
#include <limits.h>
#endif
#include "QN_Logger.h"
#include "QN_types.h"
#include "QN_streams.h"
#include "QN_RateSchedule.h"
#include "QN_MLP.h"

// A class for performing MLP training with hard targets.

class QN_HardSentTrainer
{
public:
    QN_HardSentTrainer(int a_debug, const char* a_dbgname,
		       int a_verbose, QN_MLP* a_mlp,
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
		       int a_lastlab_reject = 0,
		       float* a_lrscale = NULL);
    ~QN_HardSentTrainer();
    // Actually do training.
    void train();

protected:
    int debug;
    const char* dbgname;
    QN_ClassLogger clog;	// Logging class.
    int verbose;		// If non-zero, produce extra status msgs.
    QN_MLP* mlp;		// MLP being trained.
    const size_t mlp_inps;	// Number of inputs to the MLP.
    const size_t mlp_outs;	// Number of outputs from MLP.
    
    QN_InFtrStream* train_ftr_str; // Feature stream used for training.
    QN_InLabStream* train_lab_str; // Label stream used for training.
    QN_InFtrStream* cv_ftr_str;	// Feature stream used for cross validation.
    QN_InLabStream* cv_lab_str;	// Label stream used for cross validation.

    QN_RateSchedule* lr_sched;	// Learning rate schedule.
    const float targ_low;	// Value used for training 0 probability.
    const float targ_high;	// Value used for training 1 probability.

    const size_t bunch_size;	// Number of presentations to pass to net
				// at one time.
    const int lastlab_reject;	// Whether last label value means 
				// "do not train on this label".  1 means
				// enabled, 0 means disabled.
    const size_t inp_buf_size;	// Number of values passed to MLP.
    const size_t out_buf_size;	// Number of values output by MLP.
    const size_t targ_buf_size;	// Number of values in training target buffer.

    float* inp_buf;		// Buffer for values going into net.
    float* out_buf;		// Buffer for values coming out from net.
    float* targ_buf;		// Buffer for correct outputs when training.
    QNUInt32*lab_buf;		// One bunches worth of labels.

    char* wlog_template;	// Template used for temp weight file names.
    QN_WeightFileType wfile_format; // Format of weight file
    char* ckpt_template;	// Template use for checkpoint weight files.
    QN_WeightFileType ckpt_format; // Format of ckpt files.
    unsigned long ckpt_secs;	// Time in seconds between checkpoints.
    time_t last_ckpt_time;	// Time of last checkpoint.

    const int pid;		// Proces ID - used for building weight file
				// names.

    float learn_rate;		// The current learning rate.
    float lrscale[QN_MLP_MAX_LAYERS-1];	// Learning scale values for each sect.
    size_t epoch;		// Current epoch.

// Local functions.
    double cv_epoch();		// Do one epochs worth of cross validation.
    double train_epoch();	// Do one epochs worth of training.
    void set_learnrate();	// Set the learning rates in the net based
				// on the value of learn_rate.
    void checkpoint_weights();	// Dump a checkpoint of the weights.
};

// A class for performing MLP training with soft targets.

class QN_SoftSentTrainer
{
public:
    QN_SoftSentTrainer(int a_debug, const char* a_dbgname,
		       int a_verbose, QN_MLP* a_mlp,
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
		       float* a_lrscale = NULL);
    ~QN_SoftSentTrainer();
    // Actually do training.
    void train();

protected:
    int debug;			// Debug level.
    const char* dbgname;	// Debug object name.
    QN_ClassLogger clog;	// Logging class.
    int verbose;		// If non-zero, produce extra status msgs.
    QN_MLP* mlp;		// MLP being trained.
    const size_t mlp_inps;	// Number of inputs to the MLP.
    const size_t mlp_outs;	// Number of outputs from MLP.
    
    QN_InFtrStream* train_ftr_str; // Feature stream used for training.
    QN_InFtrStream* train_targ_str; // Label stream used for training.
    QN_InFtrStream* cv_ftr_str;	// Feature stream used for cross validation.
    QN_InFtrStream* cv_targ_str; // Label stream used for cross validation.

    QN_RateSchedule* lr_sched;	// Learning rate schedule.
    const float targ_low;	// Value used for training 0 probability.
    const float targ_high;	// Value used for training 1 probability.

    const size_t bunch_size;	// Number of presentations to pass to net
				// at one time.
    const size_t inp_buf_size;	// Number of values passed to MLP.
    const size_t out_buf_size;	// Number of values output by MLP.
    const size_t targ_buf_size;	// Number of values in training target buffer.

    float* inp_buf;		// Buffer for values going into net.
    float* out_buf;		// Buffer for values coming out from net.
    float* targ_buf;		// Buffer for correct outputs when training.

    char* wlog_template;	// Template used for temp weight file names.
    QN_WeightFileType wfile_format; // Format for weights file.
    char* ckpt_template;	// Template use for checkpoint weight files.
    QN_WeightFileType ckpt_format; // Format of ckpt files.
    unsigned long ckpt_secs;	// Time in seconds between checkpoints.
    time_t last_ckpt_time;	// Time of last checkpoint.

    const int pid;		// Proces ID - used for building weight file
				// names.

    float learn_rate;		// The current learning rate.
    float lrscale[QN_MLP_MAX_LAYERS-1];	// Learning scale values for each sect.
    size_t epoch;		// Current epoch.

// Local functions.
    double cv_epoch();		// Do one epochs worth of cross validation.
    double train_epoch();	// Do one epochs worth of training.
    void set_learnrate();	// Set the learning rates in the net based
				// on the value of learn_rate.
    void checkpoint_weights();	// Dump a checkpoint of the weights.
};

// A class for performing MLP training with soft targets.

#endif
