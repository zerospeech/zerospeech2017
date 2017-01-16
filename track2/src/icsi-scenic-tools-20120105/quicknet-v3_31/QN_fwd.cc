const char* QN_fwd_rcsid =
    "$Header: /u/drspeech/repos/quicknet2/QN_fwd.cc,v 1.13 2011/05/11 05:14:57 davidj Exp $";

// Routines for performing MLP forward passes.

#include <QN_config.h>
#include <assert.h>
#include "QN_fwd.h"
#include "QN_utils.h"
#include "QN_fltvec.h"

void
QN_hardForward(int debug, const char* dbgname, int verbose, QN_MLP* mlp,
	       QN_InFtrStream* inp_str, QN_InLabStream* inlab_str,
	       QN_OutFtrStream* out_str, QN_OutLabStream* outlab_str,
	       size_t bunch_size, int lastlab_reject)
{
    // A class for logging.
    QN_ClassLogger clog(debug, "QN_hardForward", dbgname);
  
    double start_time;		// The time the forward pass started.
    double stop_time;		// The time the forward pass finished.
    double total_time;		// The elapsed time of the forward pass.
    size_t n_inps;		// The width of the input stream.
    size_t mlp_inps;		// The number of input units in the MLP.
    size_t n_labs;		// The number of labels in the label stream.
    size_t n_outs = 0;		// The number of output units.
    size_t mlp_outs;		// The number of output units in the MLP.

    // lastlab_reject needs to be 0 of false and 1 if true
    lastlab_reject = lastlab_reject ? 1 : 0;
    // Check out the input streams.
    assert(inp_str!=NULL);
    n_inps = inp_str->num_ftrs();
    clog.log(QN_LOG_PER_RUN, "Forward pass on a stream containing %lu "
	     "features.", (unsigned long) n_inps);
    if (inlab_str!=NULL)
    {
	n_labs = inlab_str->num_labs();
	if (n_labs!=1)
	{
	    clog.error("Input label stream contains %lu labels - "
		       "can only verify against one label.", n_labs);
	}
	clog.log(QN_LOG_PER_RUN, "Verifying against a stream containing %lu "
		 "labels.", (unsigned long) n_labs);
    }
    else
	n_labs = 0;

    // Check out the output stream.
    if (out_str!=NULL)
    {
	n_outs = out_str->num_ftrs();
	clog.log(QN_LOG_PER_RUN, "Producing an output stream %lu "
		 "values wide.", (unsigned long) n_outs);
    }
    if (outlab_str!=NULL)
    {
	if (outlab_str->num_labs()!=1);
	{
	    clog.error("Output label stream requires %lu labels - we only "
		       "provide 1.", outlab_str->num_labs());
	}
    }

    // Check out the MLP.
    mlp_inps = mlp->size_layer((QN_LayerSelector) 0);
    mlp_outs = mlp->size_layer((QN_LayerSelector) (mlp->num_layers()-1));
    if (out_str==NULL)
	n_outs = mlp_outs;
    else
    {
	if (mlp_outs!=n_outs)
	{
	    clog.error("MLP has %lu outputs but output stream expects %lu.",
		       (unsigned long) mlp_outs, (unsigned long) n_outs);
	}
    }
    if (mlp_inps!=n_inps)
    {
	clog.error("MLP has %lu inputs but input stream provides %lu.",
		   (unsigned long) mlp_inps, (unsigned long) n_inps);
    }


    QN_SegID inp_segid;		// The segment ID of the current segment from
				// the input file.
    QN_SegID lab_segid;		// The segment ID of the current segment from
				// the label file.
    float* inp_buf = NULL;	// Put the data for input to the net here.
    float* out_buf = NULL;	// Buffer for output from the net.
    QNUInt32* inlab_buf = NULL;	// Put the read labels here.
    QNUInt32* outlab_buf = NULL; // Put the most likely output label here.
    size_t segno;		// Current segment number.
    unsigned long right_pres = 0; // Count of correct presentations.
    unsigned long total_pres = 0; // Count of total presentations.
    unsigned long reject_pres = 0; // Number of reject presentations.

    // Allocate buffers.
    const size_t size_inp_buf = n_inps * bunch_size;
    inp_buf = new float[size_inp_buf];
    const size_t size_out_buf = n_outs * bunch_size;
    out_buf = new float[size_out_buf];
    clog.log(QN_LOG_PER_RUN, "Allocated %lu floats for input buffer and "
	     "%lu floats for output buffer.",
	     (unsigned long) size_inp_buf, (unsigned long) size_out_buf);
    if (inlab_str!=NULL)
	inlab_buf = new QNUInt32[bunch_size];
    if (outlab_str!=NULL || inlab_str!=NULL)
	outlab_buf = new QNUInt32[bunch_size];

    start_time = QN_time();
    segno = 0;
    while(1)			// Iterate over all segments.
    {
	// Start the segment.
	inp_segid = inp_str->nextseg();
	if (n_labs!=0)
	{
	    lab_segid = inlab_str->nextseg();
	    if (inp_segid!=lab_segid)
	    {
		clog.error("Different segment identifiers in feature "
			   "and label files.");
	    }
	}
	if (inp_segid==QN_SEGID_BAD)
	    break;		// End of stream.

	size_t inp_count;	// The number of input frames read this read.
	size_t lab_count;	// The number of label frames read this read.
	do			// Iterate over all bunches in segment.
	{
	    // Read in the features.
	    inp_count = inp_str->read_ftrs(bunch_size, inp_buf);

	    // Do the actual forward operation.
	    mlp->forward(inp_count, inp_buf, out_buf);

	    // Work out the labels selected by the net.
	    if (outlab_str!=NULL || inlab_str!=NULL)
	    {
		float* out_ptr;	// A pointer to the current output vector.
		out_ptr = &out_buf[0];
		size_t i;	// Local counter.

		for (i=0; i<inp_count; i++)
		{
		    outlab_buf[i] = qn_imax_vf_u(n_outs, out_ptr);
		    out_ptr += n_outs;
		}
	    }

	    // Verify the results against the label file.
	    if (inlab_str!=NULL)
	    {
		lab_count = inlab_str->read_labs(bunch_size, inlab_buf);
		if (inp_count!=lab_count)
		{
		    clog.error("Length of segment in feature file is "
			       "different from length of segment in "
			       "label file in segment %lu.",
			       (unsigned long) segno);
		}

		// Keep statistics on how the output compared with the
		// label file.
		size_t i;	// Local counter.
		for (i=0; i<lab_count; i++)
		{
		    QNUInt32 in_label;

		    in_label = inlab_buf[i];
		    if (in_label >= (n_outs + lastlab_reject))
		    {
			clog.error("Label value of %lu is too large "
				   "in segment %lu.", in_label, segno);
		    }
		    if (lastlab_reject && in_label==n_outs)
		    {
			reject_pres++;
		    }
		    else if (in_label==outlab_buf[i])
			right_pres++;
		}
	    }
	    // Send the net outputs to a stream.
	    if (out_str!=NULL)
		out_str->write_ftrs(inp_count, out_buf);

	    // Send the calculated labels to a stream.
	    if (outlab_str!=NULL)
		outlab_str->write_labs(inp_count, outlab_buf);

	    total_pres += inp_count;
	} while (inp_count==bunch_size);

	// Finish the segment.
	if (out_str!=NULL)
	    out_str->doneseg(inp_segid);
	if (outlab_str!=NULL)
	    outlab_str->doneseg(inp_segid);
	    
	segno++;
    }
    stop_time = QN_time();
    total_time = stop_time - start_time;

    int hours = ((int) total_time) / 3600;
    int mins = (((int) total_time) / 60) % 60;
    int secs = ((int) total_time) % 60;
    QN_OUTPUT("Recognition time: %.2f secs (%i hours, %i mins, %i secs).",
	      total_time, hours, mins, secs);
    QN_OUTPUT("Recognition speed: %.2f MCPS, %.1f frames/sec.",
	    QN_secs_to_MCPS(total_time, total_pres, *mlp),
	    (double) total_pres/(double) total_time);
    if (inlab_str!=NULL)
    {
	unsigned long unreject_pres = total_pres - reject_pres;
	double percent_right = 100.0
	    * (double)right_pres / (double)unreject_pres;
	QN_OUTPUT("Recognition accuracy: %lu right out of %lu, "
		  "%.2f%% correct.",
		  (unsigned long) right_pres,
		  (unsigned long) unreject_pres,
		  percent_right);
	if (lastlab_reject)
	{
	    double percent_reject = 100.0
		* (double)reject_pres / (double) total_pres;
	    QN_OUTPUT("Reject frames: %lu of total %lu frames rejected, "
		      "%.2f%% rejected.",
		      (unsigned long) reject_pres, (unsigned long) total_pres,
		      percent_reject);
	}
    }
    if (outlab_buf!=NULL)
	delete[] outlab_buf;
    delete[] out_buf;
    if (inlab_buf!=NULL)
	delete[] inlab_buf;
    delete[] inp_buf;
}

void
QN_enumForward(int debug, const char* dbgname, int verbose, QN_MLP* mlp,
	       QN_InFtrStream* inp_str, QN_InLabStream* inlab_str,
	       QN_OutFtrStream* out_str, size_t unary_size)
{
    // A class for logging.
    QN_ClassLogger clog(debug, "QN_enumForward", dbgname);
  
    double start_time;		// The time the forward pass started.
    double stop_time;		// The time the forward pass finished.
    double total_time;		// The elapsed time of the forward pass.
    size_t n_inps;		// The width of the input stream.
    size_t mlp_inps;		// The number of input units in the MLP.
    size_t mlp_outs;		// The number of output units in the MLP.
    size_t n_labs;		// The number of labels in the label stream.
    size_t n_outs = 0;		// The number of output units.

    // Check out the input streams.
    assert(inp_str!=NULL);
    assert(out_str!=NULL);
    assert(inlab_str==NULL);
    n_inps = inp_str->num_ftrs();
    clog.log(QN_LOG_PER_RUN, "Enumerated unary input forward pass "
	     "on a stream containing %lu "
	     "features.", (unsigned long) n_inps);

    if (inlab_str!=NULL)
    {
	n_labs = inlab_str->num_labs();
	if (n_labs!=1)
	{
	    clog.error("Input label stream contains %lu labels - "
		       "can only verify against one label.", n_labs);
	}
	clog.log(QN_LOG_PER_RUN, "Verifying against a stream containing %lu "
		 "labels.", (unsigned long) n_labs);
    }
    else
	n_labs = 0;

    // Check out the output stream.
    n_outs = out_str->num_ftrs();
    clog.log(QN_LOG_PER_RUN, "Producing an output stream %lu "
	     "values wide.", (unsigned long) n_outs);

    // Check out the MLP.
    mlp_inps = mlp->size_layer((QN_LayerSelector) 0);
    mlp_outs = mlp->size_layer((QN_LayerSelector) (mlp->num_layers()-1));

    if (mlp_outs*unary_size!=n_outs)
    {
	clog.error("Output stream has %lu outputs but have %lu values.",
		   (unsigned long) n_outs, (unsigned long) (mlp_outs*unary_size));
    }
    if (mlp_inps!=n_inps+unary_size)
    {
	clog.error("MLP has %lu inputs but input stream provides %lu.",
		   (unsigned long) mlp_inps, (unsigned long) n_inps+unary_size);
    }


    QN_SegID inp_segid;		// The segment ID of the current segment from
				// the input file.
    QN_SegID lab_segid;		// The segment ID of the current segment from
				// the label file.
    float* inp_buf = NULL;	// Put the data for input to the net here.
    float* out_buf = NULL;	// Buffer for output from the net.
    QNUInt32* inlab_buf = NULL;	// Put the read labels here.
    QNUInt32* outlab_buf = NULL; // Put the most likely output label here.
    size_t segno;		// Current segment number.
    unsigned long right_pres = 0; // Count of correct presentations.
    unsigned long total_pres = 0; // Count of total presentations.

    // Allocate buffers.
    const size_t size_inp_buf = n_inps + unary_size;
    inp_buf = new float[size_inp_buf];
    const size_t size_out_buf = n_outs * unary_size;
    out_buf = new float[size_out_buf];
    clog.log(QN_LOG_PER_RUN, "Allocated %lu floats for input buffer and "
	     "%lu floats for output buffer.",
	     (unsigned long) size_inp_buf, (unsigned long) size_out_buf);
    if (inlab_str!=NULL)
	inlab_buf = new QNUInt32[1];

    start_time = QN_time();
    segno = 0;
    while(1)			// Iterate over all segments.
    {
	// Start the segment.
	inp_segid = inp_str->nextseg();
	if (n_labs!=0)
	{
	    lab_segid = inlab_str->nextseg();
	    if (inp_segid!=lab_segid)
	    {
		clog.error("Different segment identifiers in feature "
			   "and label files.");
	    }
	}
	if (inp_segid==QN_SEGID_BAD)
	    break;		// End of stream.

	size_t inp_count;	// The number of input frames read this read.
	size_t lab_count;	// The number of label frames read this read.
	do			// Iterate over all bunches in segment.
	{
	    // Read in the features.
	    inp_count = inp_str->read_ftrs(1, inp_buf);
	    if (inp_count==0)
		break;

	    // Do the actual forward operation.
	    size_t unary_inp;
	    for (unary_inp=0; unary_inp<unary_size; unary_inp++)
	    {
		qn_copy_f_vf(unary_size, 0.0f, &inp_buf[mlp_inps-unary_size]);
		inp_buf[mlp_inps-unary_size+unary_inp] = 1.0f;
		mlp->forward(1, inp_buf, &out_buf[unary_inp*mlp_outs]);
	    }

	    // Work out the labels selected by the net.
	    if (inlab_str!=NULL)
	    {
		float* out_ptr;	// A pointer to the current output vector.
		out_ptr = &out_buf[0];
		size_t i;	// Local counter.

		for (i=0; i<inp_count; i++)
		{
		    outlab_buf[i] = qn_imax_vf_u(n_outs, out_ptr);
		    out_ptr += n_outs;
		}
	    }

	    // Verify the results against the label file.
	    if (inlab_str!=NULL)
	    {
		lab_count = inlab_str->read_labs(1, inlab_buf);
		if (inp_count!=lab_count)
		{
		    clog.error("Length of segment in feature file is "
			       "different from length of segment in "
			       "label file in segment %lu.",
			       (unsigned long) segno);
		}

		// Keep statistics on how the output compared with the
		// label file.
		size_t i;	// Local counter.
		for (i=0; i<lab_count; i++)
		{
		    QNUInt32 in_label;

		    in_label = inlab_buf[i];
		    if (in_label>=n_outs)
		    {
			clog.error("Label value of %lu is too large "
				   "in segment %lu.", in_label, segno);
		    }
		    if (in_label==outlab_buf[i])
			right_pres++;
		}
	    }
	    // Send the net outputs to a stream.
	    if (out_str!=NULL)
		out_str->write_ftrs(1, out_buf);

	    total_pres += inp_count;
	} while (1);

	// Finish the segment.
	if (out_str!=NULL)
	    out_str->doneseg(inp_segid);
	    
	segno++;
    }
    stop_time = QN_time();
    total_time = stop_time - start_time;

    int hours = ((int) total_time) / 3600;
    int mins = (((int) total_time) / 60) % 60;
    int secs = ((int) total_time) % 60;
    QN_OUTPUT("Recognition time: %.2f secs (%i hours, %i mins, %i secs).",
	      total_time, hours, mins, secs);
    QN_OUTPUT("Recognition speed: %.2f MCPS, %.1f frames/sec.",
	    QN_secs_to_MCPS(total_time, total_pres*unary_size, *mlp),
	    (double) total_pres/(double) total_time);
    if (inlab_str!=NULL)
    {
	double percent_right = 100.0 * (double)right_pres / (double)total_pres;
	QN_OUTPUT("Recognition accuracy: %lu right out of %lu, "
		  "%.2f%% correct.",
		  (unsigned long) right_pres, (unsigned long) total_pres,
		  percent_right);
    }
    if (outlab_buf!=NULL)
	delete[] outlab_buf;
    delete out_buf;
    if (inlab_buf!=NULL)
	delete[] inlab_buf;
    delete[] inp_buf;
}
