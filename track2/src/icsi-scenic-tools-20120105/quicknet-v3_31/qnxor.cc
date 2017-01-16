// $Header: /u/drspeech/repos/quicknet2/qnxor.cc,v 1.27 2004/05/03 16:40:30 davidj Exp $
//
// qnxor - An XOR MLP using the QuickNet MLP class library
// 
// All this program does is train an MLP to perform the xor function.
// The net has two inputs, three hidden units and one output.
//
// Note that the QuickNet library includes various other useful classes
// for applying MLPs, especially for use in speech applications.  These are
// not demonstrated in this program.

// Must include "QN_config.h" before any other include files - including
// system ones.
#include <QN_config.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// All the classes are declared in "QuickNet.h"
#include "QuickNet.h"

int
main(int argc, char** argv)
{
    // Some constant that define this particular problem
    const float LEARNRATE = 0.125;
    const int INPUT = 2;
    const int HIDDEN = 3;
    const int OUTPUT = 2;
    const float MINRANDWEIGHT = -0.1;
    const float MAXRANDWEIGHT = 0.1;
    const float MINRANDBIAS = -0.1;
    const float MAXRANDBIAS = 0.1;
    int seed = 13;		// Weight randomization seed
    // Different possible output activations functions are available for
    // different net implementations.
    // The superset of all types is defined in "QN_types.h"
    enum QN_OutputLayerType OUTTYPE = QN_OUTPUT_SOFTMAX;


    if (argc!=1)
    {
	fprintf(stderr, "Usage: qnxor\n");
	exit(1);
    }

    float* in = new float [INPUT];
    float* out = new float [OUTPUT];
    float* target = new float [OUTPUT];

    QN_MLP* net_p;		// A pointer to the net we will use
    net_p = new QN_MLP_OnlineFl3(0, "xor", INPUT, HIDDEN, OUTPUT, OUTTYPE);

    // Use a reference to the net for convenience - no need for "->"!
    QN_MLP& net = *net_p;


/*
    // This shows how to set the weights.  We can set any arbitrary
    // section of any weight matrix, hence the large number of arguments.
    float in2hid[INPUT*HIDDEN] =
	{ -0.03125, 0, -0.078125, 0.078125, 0.0625, -0.09375 };
    float hid2out[HIDDEN*OUTPUT] =
	{ 0.078125, -0.078125, 0.0 };
    float hid_bias[HIDDEN] =
	{ 0.5, 0, -0.5 };
    float out_bias[OUTPUT] =
	{ 0.0 };

    net.set_weights(QN_MLP3_INPUT2HIDDEN, 0, 0, HIDDEN, INPUT, in2hid);
    net.set_weights(QN_MLP3_HIDDEN2OUTPUT, 0, 0, OUTPUT, HIDDEN, hid2out);
    net.set_weights(QN_MLP3_HIDDENBIAS, 0, 0, HIDDEN, 1, hid_bias);
    net.set_weights(QN_MLP3_OUTPUTBIAS, 0, 0, OUTPUT, 1, out_bias);
*/

    // Randomize the weights. This is done with a utility routine.
    QN_randomize_weights(0, seed, net, MINRANDWEIGHT, MAXRANDWEIGHT,
			 MINRANDBIAS, MAXRANDBIAS);

    // Set the learning rate of the net.  For this program, the learning rate
    // for each of the weight and bias matrices is the same, so we use a
    // simple utility routine to set them (they can be set individually
    // if necessary).
    QN_set_learnrate(net, LEARNRATE);

    float mean_error = 0.0f;	// Mean of error across one epoch (4 patterns)
    int count;			// The current pattern

    for (count=0; count<30000; count++)
    {
	int in0, in1;		// The two inputs to the net
	float error = 0.0;	// The error for each forward/backward pass

        in0 = count & 1;	// We use the lower two bits of the count
        in1 = (count & 2)>>1;	// ..as the input

	in[0] = (float) in0;
	in[1] = (float) in1;
        target[0] = (float) (in0 ^ in1);
	target[1] = 1.0f - target[0];

        net.train(1, in, target, out); // Train the net with one pattern
	error = (out[0] - target[0]) * (out[0] - target[0])
	    + (out[1] - target[1]) * (out[1] - target[1]);

	// Only display the result every 100 epochs
        if ((count % 100)==0 && count!=0)
            printf("%i %f\n", count, mean_error);

	// The next bit works out the mean error for the whole epoch
        if ((count % 4)==0)
            mean_error = 0.0;
        mean_error += (error * error)/4;
    }
    delete net_p;
    delete[] target;
    delete[] out;
    delete[] in;

    exit(0);
}
