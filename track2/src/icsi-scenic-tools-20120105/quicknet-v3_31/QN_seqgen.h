// $Header: /u/drspeech/repos/quicknet2/QN_seqgen.h,v 1.1 1996/06/12 04:12:48 davidj Exp $
//
// SeqGen is the base class for seqence generators.
//
// Sequence generators return a sequence of numbers in a given range,
// and are typically used for selecting training frames for an MLP.
// These generators do not keep track of where they are - they go round in
// circles but do not know when they get back to the beginning.

#ifndef QN_seqgen_h_INCLUDED
#define QN_seqgen_h_INCLUDED

#include <QN_config.h>
#include <assert.h>
#include "QN_types.h"


class QN_SeqGen
{
public:
    // This returns the next number in the sequence
    virtual unsigned int next() = 0;
};

////////////////////////////////////////////////////////////////
// QN_SeqGen_Sequential - generates a sequential sequence of numbers 
// increasing up to, but not including, the maximum.

class QN_SeqGen_Sequential : public QN_SeqGen
{
public:
    QN_SeqGen_Sequential(QNUInt32 a_max);
    QNUInt32 next();
private:
    const QNUInt32 maximum;	// One more than the maximum value produced.
    QNUInt32 nextval;	// The next value we will return.
};


////////////////////////////////////////////////////////////////
// QN_SeqGen_RandomNoReplace passes through all numbers with no repetition, but
// when it has done this it starts again in and repeats in the same order.

class QN_SeqGen_RandomNoReplace : public QN_SeqGen
{
public:
    QN_SeqGen_RandomNoReplace(QNUInt32 a_max, QNUInt32 a_seed);
    QNUInt32 next();
private:
    QNUInt32 nextval;		// The next value we will return + 1.
    const QNUInt32 max;		// The range of values we produce.
    const QNUInt32 xor_val;	// The value we xor with.x
};

////////////////////////////////////////////////////////////////
// QN_SeqGen_Random48 uses the Unix rand48() function to generate a sequence of
// random numbers, but with possible repetition.
//
// Note: The first number produced is the seed (if the seed was withing the
//       range of the generator)

class QN_SeqGen_Random48 : public QN_SeqGen
{
public:
    QN_SeqGen_Random48(QNUInt32 a_max, QNUInt32 a_seed);
    QNUInt32 next();
private:
    QNUInt32 maximum;	// The range of values produced
    QNUInt32 val;		// Value to return
    unsigned short xsubi[3];	// The rand48 seed
};

#endif
