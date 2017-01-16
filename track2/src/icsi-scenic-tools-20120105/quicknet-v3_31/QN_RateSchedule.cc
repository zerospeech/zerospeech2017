#ifndef NO_RCSID
const char* QN_RateSchedule_rcsid = 
    "$Header: /u/drspeech/repos/quicknet2/QN_RateSchedule.cc,v 1.10 1999/03/03 16:31:46 dpwe Exp $";
#endif

// Implementation of various learning rate schedules

/* Must include the config.h file first */
#include <QN_config.h>
#include <assert.h>
#include <stddef.h>
#include <sys/types.h>
#include "QN_types.h"
#include "QN_RateSchedule.h"

////////////////////////////////////////////////////////////////
// QN_RateSchedule_List
////////////////////////////////////////////////////////////////

QN_RateSchedule_List::QN_RateSchedule_List(const float* a_rates,
					   int a_num_rates)
  : num_rates(a_num_rates),
    current(0)
{
    int i;

    rates = new float[num_rates+1];
    for (i = 0; i<num_rates; i++)
    {
	rates[i] = a_rates[i];
	assert(rates[i] > 0.0);
    }
    rates[num_rates] = 0.0;	// Terminate with 0 weight
}

QN_RateSchedule_List::~QN_RateSchedule_List()
{
    delete rates;
}

float
QN_RateSchedule_List::get_rate()
{
    return rates[current];
}

float
QN_RateSchedule_List::next_rate(float)
{
    if (current<num_rates)
	current++;
    return rates[current];
}

const char*
QN_RateSchedule_List::get_state()
{
    // FIXME - Not yet implemented
    assert(0);
    return(NULL);
}

void
QN_RateSchedule_List::set_state(const char*)
{
    // FIXME - Not yet implemented
    assert(0);
}

////////////////////////////////////////////////////////////////
// QN_RateSchedule_NewBoB
////////////////////////////////////////////////////////////////

QN_RateSchedule_NewBoB::QN_RateSchedule_NewBoB(float start_rate,
					       float a_scale_by,
					       float a_min_derror_ramp_start,
					       float a_min_derror_stop,
					       float initial_error,
					       size_t a_max_epochs)
  : scale_by(a_scale_by),
    min_derror_ramp_start(a_min_derror_ramp_start),
    min_derror_stop(a_min_derror_stop),
    max_epochs(a_max_epochs),
    rate((a_max_epochs==0) ? 0.0f : start_rate),
    ramping(0),
    lowest_error(initial_error),
    epoch(1)
{
    assert(start_rate>0.0 && start_rate<=100.0);
    assert(a_scale_by<1.0);
    assert(a_min_derror_ramp_start>0.0);
    assert(a_min_derror_stop>0.0);
}

float
QN_RateSchedule_NewBoB::get_rate()
{
    return rate;
}

float
QN_RateSchedule_NewBoB::next_rate(float current_error)
{
    float diff_error;		// The difference between the current and
				// previous errors (positive if less error now)

    if (max_epochs!=QN_ALL && epoch>=max_epochs)
    {
	rate = 0.0f;
    }
    else
    {
	diff_error = lowest_error - current_error;
	if (current_error < lowest_error)
	    lowest_error = current_error;

	if (ramping)
	{
	    if (diff_error < min_derror_stop)
		rate = 0.0f;
	    else
		rate *= scale_by;
	}
	else
	{
	    if (diff_error < min_derror_ramp_start)
	    {
		ramping = 1;
		rate *= scale_by;
	    }
	}
	epoch++;
    }
    return rate;
}

const char*
QN_RateSchedule_NewBoB::get_state()
{
    // FIXME - Not yet implemented
    assert(0);
    return(NULL);
}

void
QN_RateSchedule_NewBoB::set_state(const char*)
{
    // FIXME - Not yet implemented
    assert(0);
}

////////////////////////////////////////////////////////////////
// QN_RateSchedule_SmoothDecay
////////////////////////////////////////////////////////////////

QN_RateSchedule_SmoothDecay::QN_RateSchedule_SmoothDecay(float a_start_rate,
							 float a_decay_factor,
							 float a_min_derror_stop,
							 size_t a_min_derror_search_epochs,
							 float initial_error,
							 size_t a_num_samples_trained,
							 size_t a_max_epochs)
  : init_rate(a_start_rate),
    decay_factor(a_decay_factor/100000.0f),
    min_derror_stop(a_min_derror_stop),
    min_derror_search_epochs(a_min_derror_search_epochs),
    numsamps(a_num_samples_trained),
    max_epochs(a_max_epochs),
    rate((a_max_epochs==0) ? 0.0f : a_start_rate),
    lowest_error(initial_error),
    epoch(1),
    search_epochs(0)
{
    assert(a_start_rate>0.0 && a_start_rate<=100.0);
    assert(a_min_derror_stop>0.0);
}

float
QN_RateSchedule_SmoothDecay::get_rate()
{
  return rate;
}

// SmoothDecay adjusts its rate as samples come in, so we just report the
// current rate, unless we exceed the max_epochs or hit the stopping
// criterion
float
QN_RateSchedule_SmoothDecay::next_rate(float current_error)
{
  float diff_error;

  if (max_epochs!=QN_ALL && epoch>=max_epochs) {
    rate = 0.0f;
  } else {
    diff_error = lowest_error - current_error;
    if (current_error < lowest_error)
      lowest_error = current_error;

    if (diff_error < min_derror_stop) {
      search_epochs++;
      if (search_epochs>=min_derror_search_epochs)
	rate = 0.0f;
    } else {
      search_epochs=0;
    }

    epoch++;
  }
  return rate;
}


// trained_on_nsamples adjusts the learning rate based on the 
// number of samples we've seen

// This rate scheme provided by Holger Schwenk (schwenk@icsi.berkeley.edu)
float 
QN_RateSchedule_SmoothDecay::trained_on_nsamples(size_t nsamp)
{
  numsamps+=nsamp;
  rate=init_rate/(1+numsamps*decay_factor);
  return rate;
}

const char*
QN_RateSchedule_SmoothDecay::get_state()
{
    // FIXME - Not yet implemented
    assert(0);
    return(NULL);
}

void
QN_RateSchedule_SmoothDecay::set_state(const char*)
{
    // FIXME - Not yet implemented
    assert(0);
}
