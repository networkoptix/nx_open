/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                    Copyright (c) 1994,1995,1996                       */
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
/*                       Author :  Simon King                            */
/*                       Date   :  October 1996                          */
/*-----------------------------------------------------------------------*/
/*                         Filter functions                              */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include "EST_math.h"
#include "sigpr/EST_filter.h"
#include "sigpr/EST_fft.h"
#include "EST_wave_aux.h"
#include "EST_TBuffer.h"
#include "sigpr/EST_Window.h"
#include "EST_error.h"

// there are multiple possible styles of this because of different
// possibilities of where to change the filter coefficients.

void lpc_filter(EST_Wave &sig, EST_FVector &a, EST_Wave &res)
{
    int i, j;
    double s;

    for (i = 0; i < sig.num_samples(); ++i)
    {
	s = 0;
	for (j = 1; j < a.n(); ++j)
	    s += a(j) * (float)sig.a_safe(i - j);

	sig.a(i) = (short) s + res.a(i);
    }
}

void inv_lpc_filter(EST_Wave &sig, EST_FVector &a, EST_Wave &res)
{
    int i, j;
    double r;

    for (i = 0; i < a.n(); ++i)
    {
	r = sig.a_no_check(i);
	for (j = 1; j < a.n(); ++j)
	    r -= a.a_no_check(j) * (float)sig.a_safe(i - j);
	res.a(i) = (short) r;
    }
    for (i = a.n(); i < sig.num_samples(); ++i)
    {
	r = sig.a_no_check(i);
	for (j = 1; j < a.n(); ++j)
	    r -= a.a_no_check(j) * (float)sig.a_no_check(i - j);
	res.a(i) = (short) r;
    }
}

/*void filter(EST_FVector &in, EST_FVector &out, EST_FVector &filter)
{
    double r;
    for (int i = 0; i < in.length(); ++i)
    {
	r = in.a(i);
	for (int j = 1; j < filter.length(); ++j)
	    r -= filter(j) * in.a(i - j);
	out[i] = r;
    }
}
*/

void inv_lpc_filter_ola(EST_Wave &in_sig, EST_Track &lpc, EST_Wave &out_sig)
{

    int i, j, k, start, end, size;
    EST_FVector filter;
    EST_FVector window_vals;
    EST_Wave in_sub, out_sub;
    
    // copy attributes and size to waveform, fill with zeros.
    out_sig.resize(in_sig.num_samples(), 1);
    out_sig.set_sample_rate(in_sig.sample_rate());
    out_sig.fill(0);
    
    for(i = 1; i < lpc.num_frames() - 1; ++i)
    {
	start = (int)((float)lpc.t(i - 1) * in_sig.sample_rate());
	end = (int)((float)lpc.t(i + 1) * in_sig.sample_rate());
	if (end > out_sig.num_samples())
	    end = out_sig.num_samples();
	size = end - start;

	lpc.frame(filter, i);
	
	if (size < filter.n())  
	    break; // basically a mismatch between lpc and waveform

	in_sig.sub_wave(in_sub, start, size);
	out_sub.resize(size);

	inv_lpc_filter(in_sub, filter, out_sub);


	int centreIndex = (int)(lpc.t(i) * (float)in_sig.sample_rate());

	EST_Window::make_window(window_vals, size, "hanning", centreIndex-start);

	//	printf( "%d %d %d (start centre end)\n", start, centreIndex, end );

	// overlap and add using hanning window on original
	for (k = 0, j = start; j < end; ++j, ++k){
	  out_sig.a_no_check(j) += 
	    (int)((float)out_sub.a_no_check(k) * 
		  window_vals.a_no_check(k));
	}
    }
}

void lpc_filter_1(EST_Track &lpc, EST_Wave & res, EST_Wave &sig)
{
    int i, j, k;
    int start, end;
    EST_FVector filter;
    float s;
//    int order = lpc.num_channels() - 1;
//    EST_Wave sig_sub, res_sub;

    sig.resize(res.num_samples());
    sig.set_sample_rate(res.sample_rate());
    sig.fill(0);

    for (start = 0, i = 0; i < lpc.num_frames() - 1; ++i)
    {
	end = int((lpc.t(i) + lpc.t(i + 1)) * (float)res.sample_rate()) / 2;
	if (end > res.num_samples())
	    end = res.num_samples();

	lpc.frame(filter, i);
//	res.sub_wave(res_sub, start, (end - start));
//	sig.sub_wave(sig_sub, start, (end - start));

	// this should really be done  by the lpc_frame filter function
	// but it needs access to samples off the start of the frame
	// in question.

	if (start < filter.n())
	    for (k = start; k < end; ++k)
	    {
		for (s = 0,j = 1; j < filter.n(); ++j)
		    s += filter.a_no_check(j) * (float)sig.a_safe(k - j);
		sig.a_no_check(k) = (short) s + res.a_no_check(k);
	    }
	else
	    for (k = start; k < end; ++k)
	    {
		s = 0;
		for (j = 1; j < filter.n(); ++j)
		    s += filter.a_no_check(j) * (float)sig.a_no_check(k - j);
		sig.a_no_check(k) = (short) s + res.a_no_check(k);
	    }
	start = end;
    }    
}



void lpc_filter_fast(EST_Track &lpc, EST_Wave & res, EST_Wave &sig)
{
    // An (unfortunately) faster version of the above.  This is about
    // three time faster than the above.  Improvements would need to be
    // done of signal access to make the above compete.
    // Note the rescaling of the residual is packaged within here too
    int i, j, k, m, n;
    int start, end;
    float s;
    int order = lpc.num_channels() - 1;
    if (order < 0) order = 0;  // when lpc as no frames
    float *buff = walloc(float,res.num_samples()+order);
    float *filt = walloc(float,order+1);
    short *residual = res.values().memory();

    sig.resize(res.num_samples(),1,0);  // no reseting of values
    sig.set_sample_rate(res.sample_rate());
    for (k=0; k<order; k++)
	buff[k] = 0;
    for (start = k, m = 0, i = 0; i < lpc.num_frames() - 1; ++i)
    {
	end = int((lpc.t(i) + lpc.t(i + 1)) * (float)res.sample_rate()) / 2;
	if (end > res.num_samples())
	    end = res.num_samples();
	for (j=1; j<lpc.num_channels(); j++)
	    filt[j]=lpc.a_no_check(i,j);
	n = j;
	for (k = start; k < end; ++k,++m)
	{
	    s = 0;
	    for (j = 1; j < n; ++j)
		s += filt[j] * buff[k-j];
	    // The 0.5 should be a parameter
	    //	    buff[k] = s + (residual[m]*0.5);
	    buff[k] = s + residual[m];
	}
	start = end;
    }
    short *signal = sig.values().memory();
    for (j=0,i=order; i < k; j++,i++)
	signal[j] = (short)buff[i];
    wfree(buff);
    wfree(filt);
}

void post_emphasis(EST_Wave &sig, float a)
{
    double last=0;
    
    for (int j = 0; j < sig.num_channels(); ++j)
	for (int i = 0; i < sig.num_samples(); i++)
	{
	    sig.a(i, j) = (int)((float)sig.a(i, j) + a * last);
	    last = (float)sig(i, j);
	//	if (absval(sig(i) > maxval)
	//	    maxval = absval(fddata[i]);
	}
}

void pre_emphasis(EST_Wave &sig, float a)
{
    float x = 0.0;
    float x_1 = 0.0;

    for (int j = 0; j < sig.num_channels(); ++j)
	for (int i = 0; i < sig.num_samples(); i++)
	{
	  x = sig.a_no_check(i, j);
	  sig.a_no_check(i, j) = 
	    sig.a_no_check(i, j) - int(a * x_1);
	  x_1 = x;
	}
}

void pre_emphasis(EST_Wave &sig, EST_Wave &out, float a)
{
    out.resize(sig.num_samples(), sig.num_channels());

    for (int j = 0; j < sig.num_channels(); ++j)
    {
	out.a_no_check(0, j) = sig.a_no_check(0, j);
	for (int i = 1; i < sig.num_samples(); i++)
	    out.a_no_check(i, j) = 
		sig.a_no_check(i, j) - int(a * (float)sig.a_no_check(i-1, j));
    }
}

void post_emphasis(EST_Wave &sig, EST_Wave &out, float a)
{
    out.resize(sig.num_samples(), sig.num_channels());

    for (int j = 0; j < sig.num_channels(); ++j)
    {
	out.a_no_check(0, j) = sig.a_no_check(0, j);
	for (int i = 1; i < sig.num_samples(); i++)
	    out.a_no_check(i, j) = 
		sig.a_no_check(i, j) + int(a * (float)sig.a_no_check(i-1, j));
    }
}

void simple_mean_smooth(EST_Wave &c, int n)
{ // simple mean smoother of order n
    int i, j, h, k=1;
    float *a = new float[c.num_samples()];
    float sum;
    h = n/2;
    
    for (i = 0; (i < h); ++i)
    {
	k = (i * 2) + 1;
	sum = 0.0;
	for (j = 0; (j < k) && (k < c.num_samples()); ++j)
	    sum += c.a_no_check(j);
	a[i] = sum /(float) k;
    }
    
    for (i = h; i < c.num_samples() - h; ++i)
    {
	sum = 0.0;
	for (j = 0; j < n; ++j)
	    sum += c.a_no_check(i - h + j);
	a[i] = sum /(float) k;
    }
    
    for (; i < c.num_samples(); ++i)
    {
	k = ((c.num_samples() - i)* 2) -1;
	sum = 0.0;
	for (j = 0; j < k; ++j)
	    sum += c.a_no_check(i - (k/2) + j);
	a[i] = sum /(float) k;
    }
    
    for (i = 0; i < c.num_samples(); ++i)
	c.a_no_check(i) = (short)(a[i] + 0.5);
    
    delete [] a;
}

void FIRfilter(EST_Wave &in_sig, const EST_FVector &numerator, 
	       int delay_correction)
{
    EST_Wave out_sig;

    out_sig.resize(in_sig.num_samples());
    out_sig.set_sample_rate(in_sig.sample_rate());
    out_sig.set_file_type(in_sig.file_type());

    FIRfilter(in_sig, out_sig, numerator, delay_correction);
    in_sig = out_sig;
}

void FIRfilter(const EST_Wave &in_sig, EST_Wave &out_sig,
	       const EST_FVector &numerator, int delay_correction)
{
    if (delay_correction < 0)
	EST_error("Can't have negative delay !\n");

    if (numerator.n() <= 0)
	EST_error("Can't filter EST_Wave with given filter");

    int i,j,n=in_sig.num_samples();
    out_sig.resize(n);

    // could speed up here with three loops :
    // 1 for first part (filter overlaps start of wave, one 'if')
    // 2 for middle part (filter always within wave, no 'if's)
    // 3 for last part (filter overlaps end of wave, one 'if')

    // To make this faster do the float conversion once and hold it
    // in a conventional array.  Note this has been checked to be 
    // safe but if you change the code below you'll have to confirm it
    // remains safe. If access through FVector was fast I'd use them
    // but this is about twice as fast.
    float *in = walloc(float,n);
    for (i=0; i < n; ++i)
	in[i] = (float)in_sig.a_no_check(i);
    float *numer = walloc(float,numerator.n());
    for (i=0; i < numerator.n(); ++i)
	numer[i] = numerator.a_no_check(i);
    float *out = walloc(float,n);

    for (i = 0; i < n; ++i)
    {
	out[i] = 0;

	int jlow=0;
	int jhigh=numerator.n();

	if(i+delay_correction >= n)
	    jlow = i + delay_correction - n + 1;

	if(i+delay_correction - jhigh < 0)
	    jhigh = i + delay_correction;

	for(j=jlow; j<jhigh; j++)
	    if (((i+delay_correction - j) >= 0) &&
		((i+delay_correction - j) < n))
		out[i] += in[i+delay_correction - j] * numer[j];
    }

    for (i=0; i<n; i++)
	out_sig.a_no_check(i) = (short)out[i];
    out_sig.set_sample_rate(in_sig.sample_rate());
    out_sig.set_file_type(in_sig.file_type());

    wfree(in);
    wfree(numer);
    wfree(out);
}

void FIR_double_filter(EST_Wave &in_sig, EST_Wave &out_sig, 
		       const EST_FVector &numerator)
{
    out_sig = in_sig;
    FIRfilter(out_sig, numerator, 0);
    reverse(out_sig);
    FIRfilter(out_sig, numerator, 0);
    reverse(out_sig);
}

EST_FVector design_FIR_filter(const EST_FVector &frequency_response, 
			      int filter_order)
{
    // frequency_response contains the desired filter reponse,
    // on a scale 0...sampling frequency
    
    // check filter_order is odd
    if((filter_order & 1) == 0){
	cerr << "Requested filter order must be odd" << endl;
	return EST_FVector(0);
    }
	
    // check frequency_response has dimension 2^N
    int N = fastlog2(frequency_response.n());
    if(frequency_response.n() !=  (int)pow(float(2.0),(float)N)){
	cerr << "Desired frequency response must have dimension 2^N" << endl;
	return EST_FVector(0);
    }

    int i;
    EST_FVector filt(frequency_response);
    EST_FVector dummy(frequency_response.n());
    for(i=0;i<dummy.n();i++)
	dummy[i] = 0.0;

    int e=slowIFFT(filt,dummy);
    if (e != 0)
    {
	cerr << "Failed to design filter because FFT failed" << endl;
	return EST_FVector(0);
    }

    EST_FVector reduced_filt(filter_order);

    int mid = filter_order/2;

    reduced_filt[mid] = filt(0);
    for(i=1; i<=mid ;i++)
    {
	// Hann window for zero ripple
	float window =  0.5 + 0.5 * cos(PI*(float)i / (float)mid);
	reduced_filt[mid+i] = filt(i) * window;
	reduced_filt[mid-i] = filt(i) * window;
    }

    return reduced_filt;
}



EST_FVector design_high_or_low_pass_FIR_filter(int sample_rate, 
					       int cutoff_freq, int order,
					       float gain1, float gain2)
{
    // change to bandpass filter .....
    
    if (sample_rate <= 0){
	cerr << "Can't design a FIR filter for a sampling rate of "
	    << sample_rate << endl;
	return EST_FVector(0);
    }
    
    int i;
    int N=10;			// good minimum size
    
    int fft_size = (int)pow(float(2.0), float(N));
    while(fft_size < order*4){	// rule of thumb !?
	N++;
	fft_size = (int)pow(float(2.0), float(N));
    }
    
    // freq response is from 0 to sampling freq and therefore
    // must be symmetrical about 1/2 sampling freq
    
    EST_FVector freq_resp(fft_size);
    int normalised_cutoff = (fft_size * cutoff_freq)/sample_rate;
    for(i=0;i<normalised_cutoff;i++){
	freq_resp[i] = gain1;
	freq_resp[fft_size-i-1] = gain1;
    }
    for(;i<fft_size/2;i++){
	freq_resp[i] = gain2;
	freq_resp[fft_size-i-1] = gain2;
    }
    
    return design_FIR_filter(freq_resp, order);
}

EST_FVector design_lowpass_FIR_filter(int sample_rate, int freq, int order)
{
    return design_high_or_low_pass_FIR_filter(sample_rate, 
					      freq, order, 1.0, 0.0);
}

EST_FVector design_highpass_FIR_filter(int sample_rate, int freq, int order)
{
    return design_high_or_low_pass_FIR_filter(sample_rate, 
					      freq, order, 0.0, 1.0);
}

void FIRlowpass_filter(const EST_Wave &in_sig, EST_Wave &out_sig, 
		       int freq, int order)
{
    EST_FVector filt = design_lowpass_FIR_filter(in_sig.sample_rate(),
						 freq, order);
    FIRfilter(in_sig, out_sig, filt, filt.n()/2);
}

void FIRlowpass_filter(EST_Wave &in_sig, int freq, int order)
{
    EST_FVector filt = design_lowpass_FIR_filter(in_sig.sample_rate(),
						 freq, order);
    FIRfilter(in_sig, filt, filt.n()/2);
}


void FIRhighpass_filter(EST_Wave &in_sig, int freq, int order)
{
    EST_FVector filt = design_highpass_FIR_filter(in_sig.sample_rate(),
						  freq,order);
    FIRfilter(in_sig, filt, filt.n()/2);
}

void FIRhighpass_filter(const EST_Wave &in_sig, EST_Wave &out_sig,
			int freq, int order)
{
    EST_FVector filt = design_highpass_FIR_filter(in_sig.sample_rate(),
						  freq,order);
    FIRfilter(in_sig, out_sig, filt, filt.n()/2);
}

void FIRlowpass_double_filter(EST_Wave &in_sig, int freq, int order)
{
    EST_FVector filt = design_lowpass_FIR_filter(in_sig.sample_rate(),
						 freq, order);
    
    FIRfilter(in_sig, filt, filt.n()/2);
    reverse(in_sig);
    FIRfilter(in_sig, filt, filt.n()/2);
    reverse(in_sig);
}

void FIRlowpass_double_filter(const EST_Wave &in_sig, EST_Wave &out_sig,
			      int freq, int order)
{
    EST_FVector filt = design_lowpass_FIR_filter(in_sig.sample_rate(),
						 freq, order);
    
    FIRfilter(in_sig, out_sig, filt, filt.n()/2);
    reverse(out_sig);
    FIRfilter(out_sig, filt, filt.n()/2);
    reverse(out_sig);
}

void FIRhighpass_double_filter(const EST_Wave &in_sig, EST_Wave &out_sig,
			       int freq, int order)
{
    EST_FVector filt = design_highpass_FIR_filter(in_sig.sample_rate(),
						  freq,order);
    
    FIRfilter(in_sig, out_sig, filt, filt.n()/2);
    reverse(out_sig);
    FIRfilter(out_sig, filt, filt.n()/2);
    reverse(out_sig);
}

void FIRhighpass_double_filter(EST_Wave &in_sig, int freq, int order)
{
    EST_FVector filt = design_highpass_FIR_filter(in_sig.sample_rate(),
						  freq,order);
    
    FIRfilter(in_sig, filt, filt.n()/2);
    reverse(in_sig);
    FIRfilter(in_sig, filt, filt.n()/2);
    reverse(in_sig);
}
