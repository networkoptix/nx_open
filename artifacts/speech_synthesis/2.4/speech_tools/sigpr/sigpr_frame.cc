/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1996                            */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                 Authors: Paul Taylor and Simon King                   */
/*                 Date   :  March 1998                                  */
/*-----------------------------------------------------------------------*/
/*               Functions operating on a single frame                   */
/*                                                                       */
/*=======================================================================*/

#include "sigpr/EST_sigpr_frame.h"
#include "sigpr/EST_fft.h"
#include "EST_inline_utils.h"
#include "EST_math.h"
#include "EST_error.h"
#include "EST_TBuffer.h"

#define ALMOST1 0.99999
#define MAX_ABS_CEPS 4.0

float Hz2Mel(float frequency_in_Hertz);
float Mel2Hz(float frequency_in_Mel);

float lpredict(float *adc, int wsize, 
		      float *acf, float *ref, float *lpc,
		      int order);
float ogi_lpc(float *adc, int wsize, int order,
		      float *acf, float *ref, float *lpc);
/*
void lpc2ref(float const *a, float *b, int c)
{
    (void) a;
    (void) b;
    (void) c;
}
*/

void convert2lpc(const EST_FVector &in_frame, const EST_String &in_type,
		 EST_FVector &out_frame)
{
    if (in_type == "sig")
	sig2lpc(in_frame, out_frame);
    else if (in_type == "lsf")
	lsf2lpc(in_frame, out_frame);
    else if (in_type == "ref")
	ref2lpc(in_frame, out_frame);
    else
	EST_error("Cannot convert coefficient type %s to lpc\n", 
		  (const char *) in_type);
}

void convert2ref(const EST_FVector &in_frame, const EST_String &in_type,
		 EST_FVector &out_frame)
{
  EST_FVector tmp;

  if (in_type == "lpc")
    lpc2ref(in_frame, out_frame);
  else if (in_type == "sig")
    {
      tmp.resize(out_frame.length());
      sig2lpc(in_frame, tmp);
      lpc2ref(tmp, out_frame);
    }
  else if (in_type == "lsf")
    {
      tmp.resize(out_frame.length());
      lsf2lpc(in_frame, tmp);
      lpc2ref(tmp, out_frame);
    }
  else
    EST_error("Cannot convert coefficient type %s to reflection coefs\n", 
	      (const char *) in_type);
}

void convert2area(const EST_FVector &in_frame, const EST_String &in_type,
		 EST_FVector &out_frame)
{
  EST_FVector tmp;

  if (in_type == "lpc")
    lpc2ref(in_frame, out_frame);
  else if (in_type == "sig")
    {
      tmp.resize(out_frame.length());
      sig2lpc(in_frame, tmp);
      lpc2ref(tmp, out_frame);
    }
  else if (in_type == "lsf")
    {
      tmp.resize(out_frame.length());
      lsf2lpc(in_frame, tmp);
      lpc2ref(tmp, out_frame);
    }
  else
    EST_error("Cannot convert coefficient type %s to reflection coefs\n", 
	      (const char *) in_type);
}

void convert2cep(const EST_FVector &in_frame, const EST_String &in_type,
		 EST_FVector &out_frame)
{
    EST_FVector tmp;

    if (in_type == "lpc")
	lpc2cep(in_frame, out_frame);
    else if (in_type == "sig")
    {
	tmp.resize(out_frame.length());
	sig2lpc(in_frame, tmp);
	lpc2cep(tmp, out_frame);
    }
    else if (in_type == "lsf")
    {
	tmp.resize(out_frame.length());
	lsf2lpc(in_frame, tmp);
	lpc2cep(tmp, out_frame);
    }
    else if (in_type == "ref")
    {
	tmp.resize(out_frame.length());
	ref2lpc(in_frame, tmp);
	lpc2cep(tmp, out_frame);
    }
    else
	EST_error("Cannot convert coefficient type %s to cepstrum coefs\n", 
		  (const char *) in_type);
}

// void convert2melcep(const EST_FVector &in_frame, const EST_String &in_type,
// 		    EST_FVector &out_frame, int num_mfccs, int fbank_order,
// 		    float liftering_parameter)
// {
//     EST_FVector tmp;

//     if (in_type == "fbank")
// 	// fbank2melcep(in_frame, out_frame);
// 	return;
//     else if (in_type == "sig")
//     {
// 	tmp.resize(out_frame.length());
// 	// incomplete !
// 	//	sig2melcep(in_frame, outframe,num_mfccs,fbank_order,liftering_parameter);
//     }
//     else
// 	EST_error("Cannot convert coefficient type %s to Mel cepstrum coefs\n", 
// 		  (const char *) in_type);
// }


void convert2lsf(const EST_FVector &in_frame, const EST_String &in_type,
		 EST_FVector &out_frame)
{
  EST_FVector tmp;

  if (in_type == "lpc")
    lpc2lsf(in_frame, out_frame);
  else if (in_type == "sig")
    {
      tmp.resize(out_frame.length());
      sig2lpc(in_frame, tmp);
      lpc2lsf(tmp, out_frame);
    }
  else if (in_type == "ref")
    {
      tmp.resize(out_frame.length());
      ref2lpc(in_frame, tmp);
      lpc2lsf(tmp, out_frame);
    }
  else
    EST_error("Cannot convert coefficient type %s to reflection coefs\n", 
	      (const char *) in_type);
}

void frame_convert(const EST_FVector &in_frame, const EST_String &in_type,
		   EST_FVector &out_frame, const EST_String &out_type)
{
  if (out_type == "lpc")
      convert2lpc(in_frame, in_type, out_frame);
  else if (out_type == "lsf")
      convert2lsf(in_frame, in_type, out_frame);
  else if (out_type == "ref")
      convert2ref(in_frame, in_type, out_frame);
  else if (out_type == "cep")
      convert2cep(in_frame, in_type, out_frame);
  else if (out_type == "area")
    convert2area(in_frame, in_type, out_frame);
  else
    EST_error("Cannot convert coefficients to type %s\n", 
	      (const char *) out_type);
}

void sig2lpc(const EST_FVector &sig, EST_FVector &acf, 
		EST_FVector &ref, EST_FVector &xlpc);

float lpredict2(EST_FVector &adc, int wsize, 
		      EST_FVector &acf, float *ref, float *lpc,
		      int order);

void sig2lpc(const EST_FVector &sig, EST_FVector &lpc)
{
    EST_FVector acf(lpc.length()), ref(lpc.length());

/*    float *fadc, *facf, *fref, *flpc;

    fadc = new float[sig.length()];
    facf = new float[lpc.length()];
    fref = new float[lpc.length()];
    flpc = new float[lpc.length()];

//    for (int i = 0; i < sig.length(); ++i)
//	adc[i] = sig(i);
    
    lpredict2(sig, sig.length(), acf, fref, flpc, lpc.length()-1);

    for (int i = 0; i < lpc.length(); ++i)
	lpc.a(i) = flpc[i];
  */  
    sig2lpc(sig, acf, ref, lpc);
}

void sig2ref(const EST_FVector &sig, EST_FVector &ref)
{
    (void)sig;
    EST_FVector acf(ref.length()), lpc(ref.length());

//    sig2lpc(sig, acf, ref, lpc);
}

void ref2area(const EST_FVector &ref, EST_FVector &area)
{
    for (int i = 1; i < ref.length(); i++)
	area[i] = (1.0 - ref(i)) / (1.0 + ref(i));
}

void ref2logarea(const EST_FVector &ref, EST_FVector &logarea)
{
    int order = ref.length() -1;
    
    for(int i = 1; i <= order; i++) 
    {
	if (ref(i) > ALMOST1) 
	    logarea[i] = log((1.0 - ALMOST1) / (1.0 + ALMOST1));
	else if (ref(i) < -ALMOST1) 
	    logarea[i] = log((1.0 + ALMOST1)/(1.0 - ALMOST1));
	else
	    logarea[i] = log((1.0 - ref(i)) / (1.0 + ref(i)));
    }
}

void ref2truearea(const EST_FVector &ref, EST_FVector &area)
{
    int order = ref.length() -1;

    area[1] = (1.0 - ref(1)) / (1.0 + ref(1));
    for(int i = 2; i <= order; i++) 
	area[i] = area[i - 1] * (1.0 - ref(i)) / (1.0 + ref(i));
}

void lpc2cep(const EST_FVector &lpc, EST_FVector &cep) 
{
    int n, k;
    float sum;
    int order = lpc.length() - 1;
    
    for (n = 1; n <= order && n <= cep.length(); n++)
    {
	sum = 0.0;
	for (k = 1; k < n; k++) 
	    sum += k * cep(k-1) * lpc(n - k);
	cep[n-1] = lpc(n) + sum / n;
    }
    
    /* be wary of these interpolated values */
    for(n = order + 1; n <= cep.length(); n++)
    {
	sum = 0.0;
	for (k = n - (order - 1); k < n; k++) 
	    sum += k * cep(k-1) * lpc(n - k);
	cep[n-1] = sum / n;
    }
    
    /* very occasionally the above can go unstable, fudge if this happens */
    
    for (n = 0; n < cep.length(); n++) 
    {
	// check if NaN -- happens on some frames of silence
	if (isnanf(cep[n]) ) cep[n] = 0.0;
	
	if (cep[n] >  MAX_ABS_CEPS){
	    cerr << "WARNING : cepstral coeff " << n << " was " << 
		cep[n] << endl;
	    cerr << "lpc coeff " << n << " = " << lpc(n + 1) << endl;
	    
	    cep[n] =  MAX_ABS_CEPS;
	}
	if (cep[n] < -MAX_ABS_CEPS) {
	    cerr << "WARNING : cepstral coeff " << n << " was " << 
		cep[n] << endl;
	    cep[n] = -MAX_ABS_CEPS;
	}
    }
}

// REORG - test this!!
void lpc2ref(const EST_FVector &lpc, EST_FVector &ref)
{

  // seem to get weird output from this - best not to use it !
  EST_error("lpc2ref Code unfinished\n");

  // LPC to reflection coefficients 
  // from code from Borja Etxebarria
  // This code does clever things with pointer and so has been
  // left using float * arrays.

  // simonk (May 99) : fixed because lpc coeffs always have energy at
  // coeff 0 - the code here would need changing is lpc coeff 0 was
  // ever made optional.
  int lpc_offset=1;

    int order = lpc.length() - 1;
    int i,j;
    float f,ai;
    float *vo,*vx;
    float *vn = new float[order];
    
    i = order - 1;
    ref[i] = lpc(i+lpc_offset);
    ai = lpc(i+lpc_offset);
    f = 1-ai*ai;
    i--;
    
    for (j=0; j<=i; j++)
	ref[j] = (lpc(j+lpc_offset)+((ai*lpc(i-j+lpc_offset))))/f;
    
    /* vn=vtmp in previous #define */
    // Check whether this should really be a pointer
    vo = new float[order];
    for (i = 0; i < order; ++i)
	vo[i] = ref(i);

    for ( ;i>0; ) 
    {
	ai=vo[i];
	f = 1-ai*ai;
	i--;
	for (j=0; j<=i; j++)
	    vn[j] = (vo[j]+((ai*vo[i-j])))/f;
	
	ref[i]=vn[i];
	
	vx = vn;
	vn = vo;
	vo = vx;
    }
    
    delete [] vn;
}

void ref2lpc(const EST_FVector &ref, EST_FVector &lpc)
{
    // Here we use Christopher Longet Higgin's algorithm converted to 
    // an equivalent by awb. It doesn't have the reverse order or
    // negation requirement.
    int order = ref.length() - 1;
    float a, b;
    int n, k;
    
    for (n=0; n < order; n++)
    {
	lpc[n] = ref(n);
	for (k=0; 2 * (k+1) <= n + 1; k++)
	{
	    a = lpc[k];
	    b = lpc[n-(k + 1)];
	    lpc[k] = a-b * lpc[n];
	    lpc[n-(k+1)] = b-a * lpc[n];
	}
    }
}


/************************************************************
 **  LPC_TO_LSF -
 **     pass the LENGTH of the LPC vector - this is the LPC
 **     order plus 1.  Must pre-allocate lsfs to length+1
 ************************************************************/
void lpc2lsf(const EST_FVector &lpc,  EST_FVector &lsf)
{
    (void) lpc;
    (void) lsf;
    EST_error("LSF Code unfinished\n");
}

void lsf2lpc(const EST_FVector &lpc,  EST_FVector &lsf)
{
    (void) lpc;
    (void) lsf;
    EST_error("LSF Code unfinished\n");
}

void sig2lpc(const EST_FVector &sig, EST_FVector &acf, 
		EST_FVector &ref, EST_FVector &lpc)
{

    int i, j;
    float e, ci, sum;
    int order = lpc.length() -1;
    EST_FVector tmp(order);
    int stableorder=-1;

    if ((acf.length() != ref.length()) ||
	(acf.length() != lpc.length()))
	EST_error("sig2lpc: acf, ref are not of lpc's order");

    //cerr << "sig2lpc order " << order << endl;

    
    for (i = 0; i <= order; i++) 
    {
	sum = 0.0;
	for(j = 0; j < sig.length() - i; j++) 
	    sum += sig.a_no_check(j) * sig.a_no_check(j + i);
	acf.a_no_check(i) = sum;
    }
    
    // find lpc coefficients 
    e = acf.a_no_check(0);
    lpc.a_no_check(0) = 1.0;

    for (i = 1; i <= order; i++) 
    {
	ci = 0.0;
	for(j = 1; j < i; j++) 
	    ci += lpc.a_no_check(j) * acf.a_no_check(i-j);
	if (e == 0)
	    ref.a_no_check(i) = ci = 0.0;
	else
	    ref.a_no_check(i) = ci = (acf.a_no_check(i) - ci) / e;
	//Check stability of the recursion
	if (absval(ci) < 1.000000) 
	{
	    lpc.a_no_check(i) = ci;
	    for (j = 1; j < i; j++) 
		tmp.a_no_check(j) = lpc.a_no_check(j) - 
		    (ci * lpc.a_no_check(i-j));
	    for( j = 1; j < i; j++) 
		lpc.a_no_check(j) = tmp.a_no_check(j);

	    e = (1 - ci * ci) * e;
	    stableorder = i;
	}
	else break;
    }
    if (stableorder != order) 
    {
	fprintf(stderr,
		"warning:levinson instability, order restricted to %d\n",
		stableorder);
	for (; i <= order; i++)
	    lpc.a_no_check(i) = 0.0;
    }

    // normalisation for frame length
    lpc.a_no_check(0) = e / sig.length();
}

void sig2pow(EST_FVector &frame, float &power)
{
    power = 0.0;
    for (int i = 0; i < frame.length(); i++)
      power += pow(frame(i), float(2.0));

    power /= frame.length();
}

void sig2rms(EST_FVector &frame, float &rms_energy)
{
    sig2pow(frame, rms_energy);
    rms_energy = sqrt(rms_energy);
}


float lpredict2(EST_FVector &adc, int wsize, 
		      EST_FVector &acf, float *ref, float *lpc,
		      int order) 
{
    int   i, j;
    float e, ci, sum;
    EST_TBuffer<float> tmp(order);
    int stableorder=-1;

    EST_FVector vref(order + 1), vlpc(order + 1);
    
    for (i = 0; i <= order; i++) 
    {
	sum = 0.0;
	for (j = 0; j < wsize - i; j++) 
	    sum += adc[j] * adc[j + i];
	acf[i] = sum;
    }    
    /* find lpc coefficients */
    e = acf[0];
    lpc[0] = 1.0;
    for(i = 1; i <= order; i++) {
	ci = 0.0;
	for(j = 1; j < i; j++) 
	    ci += lpc[j] * acf[i-j];
	ref[i] = ci = (acf[i] - ci) / e;
	//Check stability of the recursion
	if (absval(ci) < 1.000000) {
	    lpc[i] = ci;
	    for(j = 1; j < i; j++) 
		tmp[j] = lpc[j] - ci * lpc[i-j];
	    for(j = 1; j < i; j++) 
		lpc[j] = tmp[j];
	    e = (1 - ci * ci) * e;
	    stableorder = i;
	}
	else break;
    }
    if (stableorder != order) {
	fprintf(stderr,
		"warning:levinson instability, order restricted to %d\n",
		stableorder);
	for (;i<=order;i++)
	    lpc[i]=0.0;
    }   
    return(e);
}



void sig2fbank(const EST_FVector &sig,
	       EST_FVector &fbank_frame,
	       const float sample_rate,
	       const bool use_power_rather_than_energy,
	       const bool take_log)
{

    EST_FVector fft_frame;
    int i,fbank_order;
    float Hz_per_fft_coeff;

    // upper and lower limits of filter bank
    // where the upper limit depends on the sampling frequency
    // TO DO : add low/high pass filtering HERE
    float mel_low=0;
    float mel_high=Hz2Mel(sample_rate / 2);

    // FFT this frame. FFT order will be computed by sig2fft
    // FFT frame returned will be half length of actual FFT performed
    sig2fft(sig,fft_frame,use_power_rather_than_energy);

    // this is more easily understood as half the sampling
    // frequency over half the fft order, but fft_frame_length()
    // is already halved
    Hz_per_fft_coeff = 0.5 * sample_rate / fft_frame.length();

    fbank_order = fbank_frame.length();

    // store the list of centre frequencies and lower and upper bounds of
    // the triangular filters
    EST_FVector mel_fbank_centre_frequencies(fbank_order+2);
    
    mel_fbank_centre_frequencies[0]=mel_low;

    for(i=1;i<=fbank_order;i++)
	mel_fbank_centre_frequencies[i] = mel_low + 
	    (float)(i) * (mel_high - mel_low) / (fbank_order+1);

    mel_fbank_centre_frequencies[fbank_order+1]=mel_high;

    // bin FFT in Mel filters
    fft2fbank(fft_frame,
	      fbank_frame,
	      Hz_per_fft_coeff,
	      mel_fbank_centre_frequencies);
    
    if(take_log)
	for(i=0;i<fbank_frame.length();i++)
	    fbank_frame[i] = safe_log(fbank_frame[i]);

}

void sig2fft(const EST_FVector &sig,
	     EST_FVector &fft_vec,
	     const bool use_power_rather_than_energy)
{

    int i,half_fft_order;
    float real,imag;
    float window_size = sig.length();
    int fft_order = fft_vec.length();

    // work out FFT order required
    fft_order = 2;   
    while (window_size > fft_order) 
	fft_order *= 2;
    
    fft_vec = sig;
    
    // pad with zeros
    fft_vec.resize(fft_order);

    // in place FFT
    (void)fastFFT(fft_vec); 

    // of course, we only need the lower half of the fft
    half_fft_order = fft_order/2;

    for(i=0;i<half_fft_order;i++)
    {
	real = fft_vec(i*2);
	imag = fft_vec(i*2 +  1);
	
	fft_vec[i] = real * real + imag * imag;

	if(!use_power_rather_than_energy)
	    fft_vec[i] = sqrt(fft_vec(i));
	
    }
    
    // discard mirror image, retaining energy/power spectrum
    fft_vec.resize(half_fft_order);

}



void fft2fbank(const EST_FVector &fft_frame, 
	       EST_FVector &fbank_vec,
	       const float Hz_per_fft_coeff,
	       const EST_FVector &mel_fbank_frequencies)
{

    // expects "half length" FFT - i.e. energy or power spectrum
    // energy is magnitude; power is squared magnitude

    // mel_fbank_frequencies is a vector of centre frequencies
    // BUT : first element is lower bound of first filter
    //       last element is upper bound of final filter
    // i.e. length = num filters + 2

    int i,k;
    float this_mel_centre,this_mel_low,this_mel_high;
    EST_FVector filter;
    int fft_index_start;

    // check that fbank_vec and mel_fbank_frequencies lengths match
    if(mel_fbank_frequencies.length() != fbank_vec.length() + 2)
    {
	EST_error("Filter centre frequencies length (%i) is not equal to fbank order (%i) plus 2\n",mel_fbank_frequencies.length(),
		  fbank_vec.length());
	return;
    }

    // filters are computed on the fly
    for(i=0;i<fbank_vec.length();i++)
    {

	// work out shape of the i'th filter
	this_mel_low=mel_fbank_frequencies(i);
	this_mel_centre=mel_fbank_frequencies(i+1);
	this_mel_high=mel_fbank_frequencies(i+2);
	
	make_mel_triangular_filter(this_mel_centre,this_mel_low,this_mel_high,
				   Hz_per_fft_coeff,fft_frame.length(),
				   fft_index_start,filter);

 	// do filtering
	fbank_vec[i]=0.0;
	for(k=0;k<filter.length();k++)
	    fbank_vec[i] += fft_frame(fft_index_start + k) * filter(k);
    }
    
}


void fbank2melcep(const EST_FVector &fbank_vec, 
		  EST_FVector &mfcc_vec,
		  const float liftering_parameter,
		  const bool include_c0)
{
    // a cosine transform of the fbank output
    // remember to pass LOG fbank params (energy or power)

    int i,j,actual_mfcc_index;
    float pi_i_over_N,cos_xform_order,const_factor;
    float PI_over_liftering_parameter;

    if(liftering_parameter != 0.0)
	PI_over_liftering_parameter = PI / liftering_parameter;
    else
	PI_over_liftering_parameter = PI; // since sin(n.PI) == 0

    // if we are not including cepstral coeff 0 (c0) then we need
    // to do a cosine transform 1 longer than otherwise
    cos_xform_order = include_c0 ? mfcc_vec.length() : mfcc_vec.length() + 1;

    const_factor = sqrt(2 / (float)(fbank_vec.length()));

    for(i=0;i<mfcc_vec.length();i++)
    {

	actual_mfcc_index = include_c0 ? i : i+1;

	pi_i_over_N  = 
	    PI * (float)(actual_mfcc_index) / (float)(fbank_vec.length());

	for(j=0;j<fbank_vec.length();j++)
	    // j + 0.5 is because we want (j+1) - 0.5
	    mfcc_vec[i] += fbank_vec(j) * cos(pi_i_over_N * ((float)j + 0.5));
	
	mfcc_vec[i] *= const_factor;

	// liftering
	mfcc_vec[i] *= 1 + (0.5 * liftering_parameter 
			    * sin(PI_over_liftering_parameter * (float)(actual_mfcc_index)));
    }
    
}

	
void make_mel_triangular_filter(const float this_mel_centre,
				const float this_mel_low,
				const float this_mel_high,
				const float Hz_per_fft_coeff,
				const int half_fft_order,
				int &fft_index_start,
				EST_FVector &filter)
{

    // makes a triangular (on a Mel scale) filter and creates
    // a weight vector to apply to FFT coefficients

    int i,filter_vector_length,fft_index_stop;
    float rise_slope,fall_slope,this_mel;


    // slopes are in units per Mel
    // this is important - slope is linear in MEl domain, not Hz
    rise_slope = 1/(this_mel_centre - this_mel_low);
    fall_slope = 1/(this_mel_centre - this_mel_high);


    // care with rounding - we want FFT indices **guaranteed**
    // to be within filter so we get no negative filter gains
    // (irint gives the _nearest_ integer)

    // round up
    if(this_mel_low == 0)
	fft_index_start=0;
    else
	fft_index_start = irint(0.5 + (Mel2Hz(this_mel_low) / Hz_per_fft_coeff));

    // round down
    fft_index_stop = irint((Mel2Hz(this_mel_high) / Hz_per_fft_coeff) - 0.5);

    if(fft_index_stop > half_fft_order-1)
	fft_index_stop = half_fft_order-1;


    filter_vector_length = fft_index_stop - fft_index_start + 1;
    filter.resize(filter_vector_length);

    for(i=0;i<filter_vector_length;i++)
    {
	this_mel = Hz2Mel( (i + fft_index_start) * Hz_per_fft_coeff );
	
	if(this_mel <= this_mel_centre)
	{
	    filter[i] = rise_slope * (this_mel - this_mel_low);
	}
	else
	{
	    filter[i] = 1 + fall_slope * (this_mel - this_mel_centre);
	}

    }

}


float Hz2Mel(float frequency_in_Hertz)
{
   return 1127 * log(1 + frequency_in_Hertz/700.0);
}

float Mel2Hz(float frequency_in_Mel)
{
    return (exp(frequency_in_Mel / 1127) - 1) * 700;
}
