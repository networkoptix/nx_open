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
/*                      Author :  Paul Taylor                            */
/*                      Date   :  December 96                            */
/*-----------------------------------------------------------------------*/
/*                    Spectrogram Generation                             */
/*                                                                       */
/*=======================================================================*/
#include <cmath>
#include <climits>
#include <cfloat>  /* needed for FLT_MAX */
#include "EST_error.h"
#include "EST_Track.h"
#include "EST_Wave.h"
#include "sigpr/EST_Window.h"
#include "EST_Option.h"
#include "sigpr/EST_fft.h"
#include "sigpr/EST_spectrogram.h"
#include "sigpr/EST_misc_sigpr.h"


void make_spectrogram(EST_Wave &sig, EST_Track &sp, EST_Features &op)
{
  EST_Wave psig;

  EST_pre_emphasis(sig, psig, op.F("preemph"));
    
  // calculate raw spectrogram
  raw_spectrogram(sp, psig, op.F("frame_length"), op.F("frame_shift"),
		  op.I("frame_order"), op.present("slow_fft"));

  if (op.present("raw"))
  {
      cout << "no scaling\n";
      return;
  }
  // coerce the values so as to emphasis important features

  if (op.present("sp_range") || op.present("sp_wcut") || op.present("sp_bcut"))
    {
      if (!op.present("sp_range"))
	  op.set("sp_range", 1.0);

      if (!op.present("sp_wcut"))
	  op.set("sp_wcut", 1.0);

      if (!op.present("sp_bcut"))
	  op.set("sp_bcut", 0.0);
      scale_spectrogram(sp, op.F("sp_range"),op.F("sp_wcut"),op.F("sp_bcut"));
    }
}

void scale_spectrogram(EST_Track &sp, float range, float wcut, float bcut)
{
    float max, min, scale, v;
    int i, j;

    max = -FLT_MIN;
    min = FLT_MAX;

    // find min and max values
    for (i = 0; i < sp.num_frames(); ++i)
	for (j = 0; j < sp.num_channels(); ++j)
	{
	  float vv = sp.a_no_check(i, j);

	    if (vv > max)
		max = vv;
	    if (vv < min)
		min = vv;
	}
    scale = (max - min);

    // for every value:
    // 1. Effectively scale in range 0 to 1
    // 2. Impose white and black cut offs
    // 3. Rescale to 0 and 1
    // 4. scale to fit in "range"
    // this can obviously be done more efficiently

    float mag = (float)range / (float)(bcut - wcut);
    for (i = 0; i < sp.num_frames(); ++i)
	for (j = 0; j < sp.num_channels(); ++j)
	{
	    v = (((sp.a_no_check(i, j) - min) / scale) - wcut) * mag;
	    if (v > range) v = range;
	    if (v < 0.0) v = 0.0;
	    sp.a_no_check(i, j) = v;
	}
}	    

void raw_spectrogram(EST_Track &sp, EST_Wave &sig, 
		     float length, 
		     float shift, 
		     int order,
		     bool slow)
{
    int frame_length = (int) (length * (float) sig.sample_rate() +0.5);
    int frame_shift = (int) (shift * (float) sig.sample_rate() +0.5);

    EST_WindowFunc *make_window = EST_Window::creator("hamming");

    // sanity check, we can't analyse more signal than order allows.
    if (frame_length > order)
      {
	EST_warning("frame_length reduced to %f (%d samples) to fit order\n",
		    (float)order/(float) sig.sample_rate(), order);
	frame_length=order;
      }

    // enough frames to cover the entire signal
    int num_frames= (int)ceil(sig.num_samples()/(float)frame_shift);
    
    // spectrogram gets order/2 powers, the moduli of order/2
    // complex numbers
    sp.resize(num_frames, order/2, FALSE);

    EST_FVector real(order);
    EST_FVector imag(order);

      // create the window shape
    EST_TBuffer<float> window_vals(frame_length); 
    make_window(frame_length, window_vals,-1);

    for (int k = 0 ; k < num_frames ; k++)
    {
      int pos = frame_shift * k;
      int window_start = pos - frame_length/2;

      real.empty();

      // imag not used in old FFT code
      if (slow)
	imag.empty();

      EST_Window::window_signal(sig, 
			       window_vals, 
			       window_start,
			       frame_length, 
			       real, FALSE);
      
      int state = slow?power_spectrum_slow(real, imag):power_spectrum(real, imag);
      if (state != 0)
	{	
	  fprintf(stderr, "FFT Failed for frame %d\n", k);
	  for (int i = 0; i < order /2; ++i)
	    sp.a_no_check(k, i) = 0;
	}
      else
	sp.copy_frame_in(k, real);
    }
    sp.fill_time(shift);
}

