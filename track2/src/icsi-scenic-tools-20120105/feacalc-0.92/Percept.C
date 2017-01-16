//
// Percept.C
//
// Calculate "perceptual" features
// Part of feacalc project.
//
// 2001-12-28 dpwe@ee.columbia.edu based on Manuel's code
// $Header: /u/drspeech/repos/feacalc/Percept.C,v 1.1 2002/03/18 21:13:21 dpwe Exp $

#include "Percept.H"

FtrCalc_Percept::FtrCalc_Percept(CL_VALS* clvals) {
    // Initialize the processing chain

    // set up our public fields
    nfeats = (clvals->deltaOrder+1) * basefeats;
    framelen = params.winpts;
    steplen = params.steppts;
    if (clvals->zpadTime > 0) {
	padlen = (int)(clvals->zpadTime*clvals->sampleRate/1000.0);
	dozpad = 1;
	if (clvals->doPad) {
	    fprintf(stderr, "FtrCalc_Percept::FtrCalc_Percept: you can't have both zeropadding and window padding\n");
	    exit(-1);
	}
    } else {
	dozpad = 0;
	padlen  = (clvals->doPad)?((framelen-steplen)/2):0;
    }

  // Supported features so far
  pbz = clvals->pbz;
  fourier = clvals->fourier;
  extended = clvals->extended;
}

FtrCalc_Percept::~FtrCalc_Percept(void) {
    // release
}

floatRef FtrCalc_Percept::Process(/* const */ floatRef* samps) {
  int i,n,feature_number=0;  
  floatVec rtn(param.nfeats), subband_powers(4), coefficients;
  float loudness_value, bw_value, brightness_value, zcr_value, pitch;
  float spectrum_power; 
  struct fvec data, frequency_data, hamming_data;
  struct fvec *result = NULL;

  if(samps) {
      data.length = samps->Len();
      data.values = &(samps->El(0));
      hamming_data.length = samps->Len();
      //      printf("hammind_data length %d\n",hamming_data.length );
      hamming_data.values = (float *) malloc(hamming_data.length*sizeof(float));
      assert(samps->Step() == 1);
      loudness_value   = get_loudness(&data);
      if(loudness_value == 0.0)	{
	  *(isZero) = 1;
	  return rtn;
	}
      ApplyHammingDistance(&data, &hamming_data);
      if(pbz || extended) {
	  result   = powspec(&(rasta->params), &hamming_data, NULL);
	  frequency_data.length = result->length;
	  frequency_data.values = result->values;
	  spectrum_power = get_spectrum(&frequency_data); 
	  brightness_value = get_brigthness(&frequency_data, (float)exp((double)spectrum_power));     
	}
      
      if(pbz)
	{
	  pitch = get_pitch(&data);
	  //pitch = get_pitch2(&frequency_data);
	  rtn.Set(feature_number,pitch);      
	  feature_number++;
	  bw_value = get_bandwidth(&frequency_data,  brightness_value,(float)exp((double)spectrum_power));
	  rtn.Set(feature_number,bw_value);
	  feature_number++;
	  zcr_value  = get_zeroCrossing(&data);
	  rtn.Set(feature_number,zcr_value);
	  feature_number++;
	}
 
      if (extended==1)
	{
	  rtn.Set(feature_number,loudness_value);
	  feature_number++;
	  rtn.Set(feature_number,brightness_value);      
	  feature_number++;

	  subband_powers = get_subband(&frequency_data);
	  n = subband_powers.Len();

	  for(i = 0 ; i < n ; i++)
	    {
	      rtn.Set(feature_number+i,subband_powers.El(i));
	    }
	  feature_number=feature_number+n;
	}
      
      if(param.coeff!=0)
	{
	  floatRef hamming_data_vector;
	  hamming_data_vector.SetLen(hamming_data.length);
	  hamming_data_vector.SetData(hamming_data.values);
	  coefficients = rasta->Process(&hamming_data_vector);
	  if(coefficients.Len() != param.coeff)
	    {
	      printf("Error on Rasta number of coefficients is not equal to the frame length\n");
	      exit(1);
	    }
	    
	  for(i = 0; i < param.coeff; i++)
	    {
	      rtn.Set(feature_number+i,coefficients.Val(i));
	    }
	  if(feature_number+param.coeff!=param.nfeats)
	    {
	      printf("There is a problem with the Process of the features, feature dimension does not match\n");
	    }
	  free(hamming_data.values);
	  
	}
    }
  return rtn;
}


int FtrCalc_Percept::get_num_points_FFT(int n)
{
  int fftlen, log2len;
  log2len = (int) ceil(log((double)(n))/log(2.0));
  fftlen = (int)((int) pow(2.0,(double)(log2len))+.5);  

  return fftlen;
  
}


float FtrCalc_Percept::get_pitch(fvec *data_fptr)
{
  float *Seq, temp, value;
  int i, log2length, fftlength, n;
  
  //Round Up
  
  n = data_fptr->length;
  
  log2length = (int) ceil(log((double)(n))/log(2.0));
  fftlength = (int)((int) pow(2.0,(double)(log2length))+.5);
  // printf("the number of points in the thing are %d\n",fftlength);  
  Seq = (float *) malloc(2*fftlength*sizeof(float));
  
  for(i = 0; i < n; i ++)
    {
      Seq[2*i] = data_fptr->values[i];
      Seq[2*i+1] = 0;
    }
  for(i = n ; i < fftlength ; i++)
    {
      Seq[2*i] = 0;
      Seq[2*i+1] = 0;
    }
  fft_normal(Seq, (long)(2*fftlength));

  for(i = 0 ; i < fftlength ; i++)
    {
      temp = (float) sqrt((double)(Seq[2*i]*Seq[2*i] + Seq[2*i+1]*Seq[2*i+1]));
      Seq[2*i] = (float)pow((double)temp,1.5);
      Seq[2*i+1] = 0;      
    }
  ifft_normal(Seq, long(2*fftlength));
  value = (float)((float)find_Maximum(Seq, fftlength)/(float)(n));
  if(value!=0)
    value = (float)log((double)value); 
  free(Seq);
  return value;
  
}


float FtrCalc_Percept::get_correlation(int lag,  float *data, int n)
{
  float *Seq, value, temp;
  int i, log2length, fftlength, m;

  m = lag * 3;
  
  //Round Up
  
  log2length = (int) ceil(log((double)(n))/log(2.0));
  fftlength = (int)((int) pow(2.0,(double)(log2length))+.5);
  // printf("the number of points in the thing are %d\n",fftlength);  
  Seq = (float *) malloc(2*fftlength*sizeof(float));

  for(i = 0; i < n; i ++)
    {
      Seq[2*i] = data[i];
      Seq[2*i+1] = 0;
    }
  for(i = n ; i < fftlength ; i++)
    {
      Seq[2*i] = 0;
      Seq[2*i+1] = 0;
    }
  fft_normal(Seq, (long)(2*fftlength));

  for(i = 0 ; i < fftlength ; i++)
    {
      temp = (float) sqrt((double)(Seq[2*i]*Seq[2*i] + Seq[2*i+1]*Seq[2*i+1]));
      Seq[2*i] = (float)pow((double)temp,2);
      Seq[2*i+1] = 0;      
    }

  ifft_normal(Seq, long(2*fftlength));

  lag = (int) ceil((double)((double)((double)lag/100.0)*(double)(n)));
  if(lag > m)
    lag = m;
  if(Seq[0] !=0)
    value = (float)((float)Seq[2*lag]/(float)Seq[0]);
  else
    value = 0;
  free(Seq);
  return value;
}

float FtrCalc_Percept::find_Maximum(float *correlation, int n)
{
  int i;
  float ref, maximum = 0;
  float previous;

  ref = correlation[0];  
  
  for (i = 2; i < n-1 ; i+=2)
    {
      
      if((correlation[i]>correlation[i-2])&&(correlation[i]>=correlation[i+2]) &&(correlation[i]>(ref*0.65)))
	{
	  //  maximum = 2*((float)i/n);
	  maximum = (float)i/2;
	  i = n;
	}
    }
  return maximum;  
}

float FtrCalc_Percept::get_bandwidth(fvec *fptr, float brigthness, float power_spectrum)
{
  float value = 0;
  int i;
  int n;

  n = (int) (fptr->length);

  for(i = 0; i < n ; i++)
    {
      value = value + (float) pow((double)((PI*i/(fptr->length-1)) - brigthness),2) * fptr->values[i]; 
    }

  value = (float) value/power_spectrum;
  value = (float)sqrt((double)value);
  
  return value;    
}

float FtrCalc_Percept::get_brigthness(fvec *fptr, float power_spectrum)
{
  float value = 0;
  int i;
  int n;

  n = (int) fptr->length;

  for(i = 0; i < n ;i++)
    {
      value = value + (float) (PI*i/(fptr->length-1))  * (float) fptr->values[i]; 
    }

  value = (float) value/power_spectrum;
  
  return value;    
}

float FtrCalc_Percept::get_spectrum(fvec *fptr)
{
  float value = 0;
  int i;
  int n;

  n = (int) (fptr->length);

  for(i = 0; i < n ;i++)
    {
      value = value + (float) fptr->values[i]; 
    }

  value = (float)log((double)value);
  
  return value;    
}

struct floatVec FtrCalc_Percept::get_subband(fvec *fptr)
{
  struct floatVec subband(4);
  float value=0;
  int n,m, i;

  n = (int) floor((double)(fptr->length/8));
  
  for(i = 0; i < n; i++)
    {
      value = value + fptr->values[i]; 
    }

  value = (float)log((double)value);
  subband.Set(0,value);    
  value = 0;
  m = (int) floor((double)(fptr->length/4));

  for(i =  n; i < m; i++)
    {
      value = value + (float) fptr->values[i]; 
    }
  value = (float)log((double)value);
  subband.Set(1,value);
  value = 0;
  n = (int) floor((double)(fptr->length/2));

  for(i =  m; i < n; i++)
    {
      value = value + (float) fptr->values[i]; 
    }
  value = (float)log((double)value);
  subband.Set(2,value);
  value = 0;
  m = (int) floor((double)(fptr->length));

  for(i =  n; i < m; i++)
    {
      value = value + (float) fptr->values[i]; 
    }
  value = (float)log((double)value);
  subband.Set(3,value);
  
  return subband;    
}

float FtrCalc_Percept::get_loudness(fvec *fptr)
{
  float value=0;
  int i;

#ifdef DEBUG
  printf("The length of samples is %d\n", fptr->length);
#endif /* DEBUG */

  for(i = 0; i < fptr->length; i++)
    {
      value = value + (float) (pow((double)fptr->values[i],2));
    }

#ifdef DEBUG
  printf("The loudness value (before dividing) is %f\n", value);
#endif /* DEBUG */

  value = (float) value/param.framelen;  
  return value;
  
}

float FtrCalc_Percept::get_zeroCrossing(fvec *fptr)
{
  float zcr=0;
  int flag;
  int i;

  if (fptr->values[0] > 0)
    { flag = 1;}
  else
    {flag = -1;}
  for(i=1;i<fptr->length;i++)
    {
      if (fptr->values[i] > 0 )
	{
	  if(flag == -1)
	    zcr++;
	  flag = 1;
	}
      if (fptr->values[i] <= 0)
	{
	  if(flag == 1)
	    zcr++;
	  flag = -1;
	}            
    }
  return zcr;
}

void FtrCalc_Percept::setParameters(CL_VALS* clvals, Rasta *rasta)
{
  param.nfeats = 0;
  
  param.framelen = (int)((double)clvals->sampleRate * (double)clvals->windowTime
                       / 1000.);
  param.steplen = (int)((double)clvals->sampleRate * (double)clvals->stepTime
                        / 1000.);

  if(pbz)
    param.nfeats+=3;
  
  if(extended)
    param.nfeats+=6;
  
  param.coeff = 0;
  if (clvals->fourier != 0)
    {
      param.coeff = rasta->nfeats;
      param.nfeats += rasta->nfeats;
    }

  if (clvals->zpadTime > 0) {
    param.padlen = (int)(clvals->zpadTime*clvals->sampleRate/1000.0);
    param.dozpad = 1;
    if (clvals->doPad) {
      fprintf(stderr, "FtrCalc_Percept::setParameters: you can't have both zeropadding and window padding\n");
      exit(-1);
    }
  } else {
    param.dozpad = 0;
    param.padlen = 0;
    
    //    param.padlen  = (clvals->doPad)?((param.framelen-param.steplen)/2):0;
  }



  // param.smallmask = clvals->doDither;
  // param.nyqhz = clvals->nyqRate;
  //param.sampfreq = clvals->sampleRate;

  return;
  
}

void FtrCalc_Percept::ApplyHammingDistance(fvec * fptr_data, fvec * fptr_hamming_data)
{
  
  int i, n = fptr_data->length;
  double a = 0.54;
  double b = 1 - a;
  
  for (i = 0 ; i < n ; i++)
    {
      fptr_hamming_data->values[i] = fptr_data->values[i]*(a - b * cos(2*M_PI*((double)i)/(n-1)));
   
    }
  
}
StaticVec FtrCalc_Percept::computeMeanAndVariance(floatVec frame, int nframes)  
{
  StaticVec statistics;
  
  

  return statistics;
  
}

