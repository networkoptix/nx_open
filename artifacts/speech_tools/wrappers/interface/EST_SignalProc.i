/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                 (University of Edinburgh, UK) and                     */
/*                           Korin Richmond                              */
/*                         Copyright (c) 2003                            */
/*                         All Rights Reserved.                          */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*                                                                       */
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
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                                                                       */
/*             Author :  Korin Richmond                                  */
/*               Date :  25 July 2003                                    */
/* -------------------------------------------------------------------   */
/*   EST_Wave and EST_Track signal processing functions                  */
/*                                                                       */
/*************************************************************************/

%module EST_SignalProc

%{
#include "sigpr/EST_filter.h"
#include "sigpr/EST_filter_design.h"
%}

%include "EST_typemaps.i"
%import "EST_Wave.i"


void FIRfilter(EST_Wave &in_sig, const EST_FVector &numerator, 
	       int delay_correction=0);

void FIRfilter(const EST_Wave &in_sig, EST_Wave &out_sig,
	       const EST_FVector &numerator, int delay_correction=0);

void FIR_double_filter(EST_Wave &in_sig, EST_Wave &out_sig, 
		       const EST_FVector &numerator);

void FIRlowpass_filter(EST_Wave &sigin, int freq, int order=DEFAULT_FILTER_ORDER);

void FIRlowpass_filter(const EST_Wave &in_sig, EST_Wave &out_sig,
		       int freq, int order=DEFAULT_FILTER_ORDER);

void FIRhighpass_filter(EST_Wave &in_sig, int freq, int order);

void FIRhighpass_filter(const EST_Wave &sigin, EST_Wave &out_sig,
			int freq, int order=DEFAULT_FILTER_ORDER);

void FIRhighpass_double_filter(EST_Wave &sigin, int freq, 
			      int order=DEFAULT_FILTER_ORDER);

void FIRhighpass_double_filter(const EST_Wave &int_sig, EST_Wave &out_sig,
			       int freq, int order=DEFAULT_FILTER_ORDER);

void FIRlowpass_double_filter(EST_Wave &sigin, int freq, 
			      int order=DEFAULT_FILTER_ORDER);

void FIRlowpass_double_filter(const EST_Wave &in_sig, EST_Wave &out_sig,
			      int freq, int order=DEFAULT_FILTER_ORDER);


void lpc_filter(EST_Wave &sig, EST_FVector &a, EST_Wave &res);


void inv_lpc_filter(EST_Wave &sig, EST_FVector &a, EST_Wave &res);

void lpc_filter_1(EST_Track &lpc, EST_Wave & res, EST_Wave &sig);

void lpc_filter_fast(EST_Track &lpc, EST_Wave & res, EST_Wave &sig);

void inv_lpc_filter_ola(EST_Wave &sig, EST_Track &lpc, EST_Wave &res);

void pre_emphasis(EST_Wave &sig, float a=DEFAULT_PRE_EMPH_FACTOR);

void pre_emphasis(EST_Wave &sig, EST_Wave &out, 
		  float a=DEFAULT_PRE_EMPH_FACTOR);

void post_emphasis(EST_Wave &sig, float a=DEFAULT_PRE_EMPH_FACTOR);

void post_emphasis(EST_Wave &sig, EST_Wave &out, 
		   float a=DEFAULT_PRE_EMPH_FACTOR);

void simple_mean_smooth(EST_Wave &c, int n);


EST_FVector design_FIR_filter(const EST_FVector &freq_response, 
			      int filter_order);

EST_FVector design_lowpass_FIR_filter(int sample_rate, 
				      int freq, int order);

EST_FVector design_highpass_FIR_filter(int sample_rate, 
				       int freq, int order);
