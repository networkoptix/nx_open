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
/*                    Author :  Paul Taylor                              */
/*                    Date   :  July 1994                                */
/*-----------------------------------------------------------------------*/
/*                    Smooth F0 contours                                 */
/*=======================================================================*/
/*#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>*/
//#include "sigpr/EST_pda.h"
#include "EST_Track.h"
#include "EST_Features.h"
#include "array_smoother.h"
#include "EST_math.h"

void smooth_portion(EST_Track &c, EST_Features &op);
static void interp(const EST_Track &c, const EST_Track &speech, int fill,
		   EST_Track &interp);
static int parse_ms_list(EST_Features &al, struct Ms_Op *ms);
struct Ms_Op *default_ms_op(struct Ms_Op *ms);

void smooth_phrase(EST_Track &fz, EST_Track &speech, EST_Features &op, 
		   EST_Track &smi_fz)
{
    int n=0;
    EST_Track sm_fz;
    char nstring[10];

    if (fz.empty())
    {
	smi_fz = fz;
	return;
    }
    sm_fz = fz;
    sm_fz.set_channel_name("F0", 0);

    n = (int)(op.F("window_length") / fz.shift());
    sprintf(nstring, "%d", n);
    op.set("point_window_size", nstring);

    if (!op.present("icda_no_smooth"))
	smooth_portion(sm_fz, op);

    if (op.present("icda_no_interp"))
    {
	sm_fz = fz;
	return; // no unvoiced interpolation
    }

    int fill = op.present("icda_fi") ? 1 : 0;
    interp(sm_fz, speech, fill, smi_fz); // fill unvoiced region

    n = (int)(op.F("second_length") / fz.shift());
    sprintf(nstring, "%d", n);
    op.set("point_window_size", nstring);

    if (!op.present("icda_no_smooth"))
	smooth_portion(smi_fz, op);
}

void smooth_portion(EST_Track &c, EST_Features &op)
{
    int i;
    float *a;  // need float * so it can be passed to array_smoother
    struct Ms_Op *ms;
    ms = new Ms_Op;

    default_ms_op(ms);
    parse_ms_list(op, ms);

    if (op.present("point_window_size"))
	ms->window_length = op.I("point_window_size");

    a = new float[c.num_frames()];
    
    for (i = 0; i < c.num_frames(); ++i)
	a[i] = c.track_break(i) ? -1.0 : c.a(i);

    array_smoother(a, c.num_frames(), ms);

    for (i = 0; i < c.num_frames(); ++i)
    {   // occasionally NaNs result...
	if (isnanf(a[i]))
	{
	    c.set_break(i);
	    c.a(i) = 0.0;
	}
	else
	{
	    if (a[i] < 0.0)
		c.set_break(i);
	    else
		c.set_value(i);
	    c.a(i) = a[i];
	}
    }

    delete a;
}

static void interp(const EST_Track &c, const EST_Track &speech, int fill,
		   EST_Track &interp)
{
    // Interpolate between unvoiced sections, and ensure breaks
    // during silences
    int i, n, p;
    float m;
    float n_val, p_val;
    float f = c.shift();

    interp = c;  // copy track

    if (speech.num_frames() < c.num_frames())
	interp.resize(speech.num_frames(), interp.num_channels());


    for (i = 1; i < interp.num_frames(); ++i)
    {
	if ((fill == 1) || (speech.a(i) > 0.5))
	{
	    if (!interp.track_break(i))
		continue;  // already has a value

	    p = i - 1;
	    if ((n = interp.next_non_break(i)) == 0)
		n = interp.num_frames() - 1;
	    n_val = interp.a(n);
	    p_val = interp.a(p);
	    if (n_val <= 0) n_val = p_val;
	    if (p_val <= 0) p_val = n_val;
	    // if they are both zero, well we'll learn to live it.
	    m = (n_val - p_val) / ( interp.t(n) - interp.t(p));

	    interp.a(i) = (m * f) + p_val;
	    interp.set_value(i);
	}
	else
	    interp.set_break(i);
    }
}

int parse_ms_list(EST_Features &al, struct Ms_Op *ms)
{
    default_ms_op(ms);
    
    if (al.present("smooth_double"))
	ms->smooth_double = al.I("smooth_double");
    if (al.present( "hanning"))
	ms->apply_hanning = al.I("hanning");
    if (al.present("extrapolate"))
	ms->extrapolate = al.I("extrapolate");
    if (al.present("first_length"))
	ms->first_median = al.I("first_length");
    if (al.present("second_length"))
	ms->second_median = al.I("second_length");
    if (al.present("window_length"))
	ms->window_length = al.I("window_length");

    return 0;
}
