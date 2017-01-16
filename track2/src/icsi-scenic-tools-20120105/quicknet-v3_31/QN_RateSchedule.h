// $Header: /u/drspeech/repos/quicknet2/QN_RateSchedule.h,v 1.7 1999/03/03 16:31:37 dpwe Exp $

#ifndef QN_RateSchedule_h_INCLUDED
#define QN_RateSchedule_h_INCLUDED

#include <QN_config.h>
#include "QN_types.h"

// The "QN_RateSchedule" class is used to control the learning rate of
// an MLP.  The class uses an "error" value to decide what the next
// learning rate should be.
//
// There are also functions to get and restore state - these could be used
// in a restartable MLP program.

class QN_RateSchedule
{
public:
    // Return the learning rate for the current epoch.
    virtual float get_rate() = 0;

    // Given the "error" for the current epoch, this returns the learning
    // rate to use for the next epoch.  A value of 0.0 means finished.
    virtual float next_rate(float error) = 0;

    // Tell the learning rate schedule that these number of samples
    // have been trained on, and return the current learning rate.  
    // Schedule implementations are free to ignore this information; 
    // a default is provided for ignoring this information.
    virtual float trained_on_nsamples(size_t nsamp) = 0;

    // Return the "state" of the learning rate schedule.  This is an ASCII
    // string that can be passed to set_state if we want to return to this
    // point at a later time.
    virtual const char* get_state() = 0;

    // Return the learning rate schedule to the state it was when the supplied
    // string was returned by get_state().
    virtual void set_state(const char* state) = 0;
};

////////////////////////////////////////////////////////////////
// This is a learning rate schedule that uses a list of learning rates - about
// as simple as it gets.
////////////////////////////////////////////////////////////////

class QN_RateSchedule_List : public QN_RateSchedule
{
public:
    // Constructor - 'num_rates' indicates the length of 'rates'
    QN_RateSchedule_List(const float* rates, int num_rates);
    virtual ~QN_RateSchedule_List();
    float get_rate();
    float next_rate(float error);
    float trained_on_nsamples(size_t nsamp);
    // Saving and restoring state
    const char* get_state();
    void set_state(const char* state);
private:
    float* rates;
    int num_rates;

    // The follwoing variables represent the dynamic state of the learning
    // rate schedule, and must be saved with get_state()
    int current;
};



////////////////////////////////////////////////////////////////
// This class is used to mimic the "new" learning rate schedule of BoB.
// In this scheme, we stay at the starting learning rate until the
// difference in successive error percentages is less than a given
// amount (min_error_ramp_start).  Then, we scale down the learning rate
// every epoch until the difference in errors is less than another
// amount (min_error_stop), and we terminate.  If we ever hit a_max_epoch
// epochs, we stop then anyway (QN_ALL means ignore this value,
// 0 means first learning rate will be 0.0).
////////////////////////////////////////////////////////////////

class QN_RateSchedule_NewBoB : public QN_RateSchedule
{
public:
    QN_RateSchedule_NewBoB(float start_rate,
			   float a_scale_by,
			   float a_min_derror_ramp_start,
			   float a_min_derror_stop,
			   float intial_error = 100.0,
			   size_t a_max_epochs = QN_ALL);
    virtual ~QN_RateSchedule_NewBoB() {};
    float get_rate();
    float next_rate(float error);
    float trained_on_nsamples(size_t nsamp);

    // Saving and restoring state
    const char* get_state();
    void set_state(const char* state);
private:
// These are paramaters that control the learning rate schedule.
    const float scale_by;
    const float min_derror_ramp_start;
    const float min_derror_stop;
    size_t max_epochs;		// The maximum number of epochs.
    
// This is the current (dynamic) state of the learning rate schedule.
    float rate;                 // The current learning rate.
    int ramping;		// 1 if the learning rate is decreasing.
    float lowest_error;		// The best error we have seen so far.
    size_t epoch;		// The current epoch number.
};

class QN_RateSchedule_SmoothDecay : public QN_RateSchedule
{
public:
  QN_RateSchedule_SmoothDecay(float a_start_rate,
			      float a_decay_factor,
			      float a_min_derror_stop,
			      size_t a_min_derr_search_epochs=1,
			      float intial_error = 100.0,
			      size_t a_num_samples_trained=0,
			      size_t a_max_epochs=QN_ALL);
  virtual ~QN_RateSchedule_SmoothDecay() {};
  float get_rate();
  float next_rate(float error);

  float trained_on_nsamples(size_t nsamp);

  const char* get_state();
  void set_state(const char *state);
private:
  // these are the parameters that control the learning rate schedule
  const float init_rate;
  const float decay_factor;
  const float min_derror_stop;
  const size_t min_derror_search_epochs;
  size_t numsamps;
  size_t max_epochs;


  // This is the current (dynamic) state of the learning rate schedule
  float rate;
  float lowest_error;
  size_t epoch;
  size_t search_epochs;
};

inline float QN_RateSchedule_List::trained_on_nsamples(size_t nsamp) {
  assert(nsamp==nsamp); // avoid compiler warnings
  return get_rate();
}

inline float QN_RateSchedule_NewBoB::trained_on_nsamples(size_t nsamp) {
  assert(nsamp==nsamp); // avoid compiler warnings
  return get_rate();
}

#endif

