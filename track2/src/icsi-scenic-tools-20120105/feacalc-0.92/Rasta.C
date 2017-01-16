//
// Rasta.C
//
// C++ encapsulation of rasta processing chain (interface to old C code)
// Part of feacalc project.
//
// 1997jul28 dpwe@icsi.berkeley.edu
// $Header: /u/drspeech/repos/feacalc/Rasta.C,v 1.13 2012/01/05 20:30:15 dpwe Exp $

#include "Rasta.H"

typedef struct map_param MAP_PARAM;

FtrCalc_Rasta::FtrCalc_Rasta(CL_VALS* clvals) : FtrCalc(clvals) {
    // Initialize the processing chain

    // rasta will call getopts on the argc/argv we pass: On Linux, 
    // it crashes if they're empty.
    int argc = 1;
    char pname[32];
    char *argv[] = { pname, NULL };

    strcpy(pname, "dummy-rasta");  /* to avoid problems with const char */

    get_comline(&params, argc, argv);

    params.deltaorder  = clvals->deltaOrder;
    params.deltawindow = clvals->deltaWindow;
    params.sampfreq    = (int)(clvals->sampleRate);
    params.nyqhz       = clvals->nyqRate;
    params.winsize     = clvals->windowTime;
    params.stepsize    = clvals->stepTime;
    //    params.HPfilter    = clvals->doHpf;
    params.HPfilter    = (clvals->hpf_fc_int > 0);
    params.hpf_fc      = (clvals->hpf_fc_int/(float)1000);
    params.smallmask   = clvals->doDither;
    params.history     = clvals->readhist;
    params.fcupper     = clvals->fcupper;
    params.fclower     = clvals->fclower;
    params.useMel      = clvals->doMel;
    params.trapezoidal = clvals->filtershape;
    params.nfilts      = clvals->nFilts;
    if (clvals->mapfile && strlen(clvals->mapfile)) {
	params.mapcoef_fname = clvals->mapfile;
    }
    if (clvals->plp != PLP_NONE && clvals->plp != PLP_DEFAULT) {
	params.order = clvals->plp;
    }
    rasta = clvals->rasta;	// to refer to in Process()
    plp   = clvals->plp;
    cmpwt = clvals->cmpwt;
    domain= clvals->domain;
    slimspect = clvals->slimspect;
    pmapping_param = NULL;
    if(clvals->rasta == RASTA_LOG) {
	params.lrasta = TRUE;
    } else if(clvals->rasta == RASTA_JAH) {
	params.jrasta = TRUE;	
	// Also, set up the mapping param structure
	pmapping_param = new MAP_PARAM;	// clears it too, right?
    } else if(clvals->rasta == RASTA_CJAH) {
	params.jrasta = TRUE;	
	params.cJah = TRUE;
	params.jah = clvals->jah;
    }
    if (clvals->histpath && strlen(clvals->histpath)) {
	params.hist_fname = clvals->histpath;
    }
    if (domain == DOMAIN_CEPSTRA) {
	if(clvals->cepsorder == 0) {
	    if(clvals->plp == PLP_NONE) {
		// cepstrum without plp - take order from default PLP order
		clvals->cepsorder = TYP_MODEL_ORDER+1;
	    } else {
		clvals->cepsorder = params.order+1;
	    }
	}
	params.nout = clvals->cepsorder;
    }

    check_args( &params ); /* Exits if params out of range */

    // init_param needs an fvec to tell it the length of the input sound
    struct fvec *sptr = alloc_fvec(0);
    init_param(sptr, &params);
    free_fvec(sptr);

//    int basefeats = (domain==DOMAIN_CEPSTRA)?params.order+1:params.nfilts;
    int basefeats = (domain==DOMAIN_CEPSTRA)?clvals->cepsorder:(params.nfilts - (slimspect?(2*params.first_good):0));

    // an fvec we can use if we need it
    goodchans = alloc_fvec(basefeats);

    // Initialization per rasta.c...

    // Initialize delta calculator
    dc = alloc_DeltaCalc(basefeats, params.deltawindow, 
			 params.deltaorder, params.strut_mode);

    // Initialize the history
    history.eof = FALSE;
    clvals->vtnWarp = 1.0;
    Reset(clvals);
    
    // set up our public fields
    nfeats = (clvals->deltaOrder+1) * basefeats;
    framelen = params.winpts;
    steplen = params.steppts;
    if (clvals->zpadTime > 0) {
	padlen = (int)(clvals->zpadTime*clvals->sampleRate/1000.0);
	dozpad = 1;
	if (clvals->doPad) {
	    fprintf(stderr, "FtrCalc_Rasta::FtrCalc_Rasta: you can't have both zeropadding and window padding\n");
	    exit(-1);
	}
    } else {
	dozpad = 0;
	padlen  = (clvals->doPad)?((framelen-steplen)/2):0;
    }
}

FtrCalc_Rasta::~FtrCalc_Rasta(void) {
    // release
    free_DeltaCalc(dc);
    if(params.history == TRUE) {
	save_history(&history, &params);
    }
    if (pmapping_param) {
	delete pmapping_param;
	pmapping_param = NULL;
    }
    if (goodchans) {
	free_fvec(goodchans);
    }
}

floatRef FtrCalc_Rasta::Powspec(floatRef* samps) {
    // Just call powspec.  First part of process
    floatRef rtn(0);
    struct fvec data;
    struct fvec *plusdeltas, *result = NULL;
    struct fmat *map_source;
    int mapset;

    if(samps) {
	data.length = samps->Len();
	data.values = &(samps->El(0));
	assert(samps->Step() == 1);

	/* result = rastaplp( &history, &params, &data ); */
	// from rasta-v2_4:anal.c:rastaplp() :
	/* Compute the power spectrum from the windowed input */
	result   = powspec(&params, &data, NULL /*2001-03-16*/);
    }
    if(result->length) {
	rtn.SetLen(result->length);
	rtn.SetData(result->values);
    }

    return rtn;
}


floatRef FtrCalc_Rasta::Process(/* const */ floatRef* samps) {
    floatRef rtn(0);
    struct fvec data;
    struct fvec *plusdeltas, *result = NULL;
    struct fmat *map_source;
    int mapset;

    if(samps) {
	data.length = samps->Len();
	data.values = &(samps->El(0));
	assert(samps->Step() == 1);

	/* result = rastaplp( &history, &params, &data ); */
	// from rasta-v2_4:anal.c:rastaplp() :
	/* Compute the power spectrum from the windowed input */
	result   = powspec(&params, &data, NULL /*2001-03-16*/);
	/* Compute critical band (auditory) spectrum */
	result = audspec(&params, result);

	if (rasta != RASTA_NONE) {
	    if (rasta == RASTA_JAH) {
		struct fvec *npower;
		/* Jah Rasta with spectral mapping */
		npower = comp_noisepower(&history, &params, result);
		if (npower->values[0]  < TINY) {
		    params.jah = 1.0e-6;
		} else {
		    params.jah = 1/(C_CONSTANT * npower->values[0]); 
		}
		comp_Jah(&params, pmapping_param, &mapset);
	    }
	    /* Put into some nonlinear domain */
	    result = nl_audspec(&params, result);
	    /* Do rasta filtering */
	    result = rasta_filt(&history, &params, result);
	    if (rasta == RASTA_JAH) {
		map_source = cr_map_source_data(&params, pmapping_param, 
						result);
		do_mapping(&params, pmapping_param, 
			   map_source, mapset, result);  
	    }
	    /* Take the quasi inverse on the nonlinearity */
	    result = inverse_nonlin(&params, result);
	}
	if(cmpwt) {
	    /* Do final cube-root compression and equalization */
	    result = post_audspec(&params, result);
	} else {
	    // not using post_audspec - need to fix up the 'edge bands' ourself
	    int i;
	    for(i = 0; i < params.first_good; ++i) {
		result->values[i] = result->values[params.first_good];
		result->values[params.nfilts-1-i] = 
		    result->values[params.nfilts-1-params.first_good];
	    }
	}

	if(plp) {
	    /* Code to do final smoothing; initial implementation
	       will only permit plp-style lpc-based representation,
	       which will consist of inverse dft followed
	       by autocorrelation-based lpc analysis and an
	       lpc to cepstrum recursion. */
	    //    result = lpccep(&params, result);
	    //    result = cep2spec(&params, result);
	    result = spec2lpc(&params, result);
#ifdef DIRECT_PLPCEP
	    /* maybe go straight from lpcoefs to cepstra? 
	       - faster and much closer numeric match to original rasta */
	    if (domain == DOMAIN_CEPSTRA) {
		result = lpc2cep(&params, result);
	    } else 
#endif /* DIRECT_PLPCEP */
		   {
		result = lpc2spec(&params, result);
	    }
	}
	if(domain == DOMAIN_CEPSTRA) {
#ifdef DIRECT_PLPCEP
	    /* already got cepstra from plp, no need to repeat */
	    if(!plp)
#endif /* DIRECT_PLPCEP */
	             {
		result = spec2cep(result, params.nout, params.lift);
	    }
	} else {
	    /* plain spectral domain.  If requested, slim down the bad chans */
	    if (slimspect) {
		int i;
		for(i = 0; i < goodchans->length; ++i) {
		    goodchans->values[i] = result->values[params.first_good+i];
		}
		result = goodchans;
	    }
	    if (domain == DOMAIN_LOG) {
		result = tolog(result);
	    }  /* else stays linear */
	}
    }

    // Offer even the NULL result to DeltaCalc, as it needs to flush its state
    plusdeltas = DeltaCalc_appendDeltas(dc,result);
    if(plusdeltas->length) {
	rtn.SetLen(plusdeltas->length);
	rtn.SetData(plusdeltas->values);
    }

    return rtn;
}

void FtrCalc_Rasta::Reset(CL_VALS *clvals) {
    // Reset all the processing state, after rasta.c
    float vtnwarp = clvals->vtnWarp;

    // Rebuild the frequency axis mapping
    params.f_warp = vtnwarp;
    set_freq_axis(&params);

    history.normInit = TRUE;
    if (params.history == TRUE) {
	load_history(&history, &params);
    }
    history.eos = TRUE;
    rasta_filt(&history, &params, (struct fvec*) NULL);
    
    if ((params.jrasta == TRUE ) && (params.cJah == FALSE)) {
	comp_noisepower(&history, &params, (struct fvec*) NULL);
    }
    /* flush deltas state */
    DeltaCalc_reset(dc);
    history.eos = FALSE;
}
