/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1997                            */
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
/*                      Author :  Paul Bagshaw                           */
/*                      Date   :  1993                                   */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* The above copyright was given by Paul Bagshaw, he retains             */
/* his original rights                                                   */
/*                                                                       */
/*************************************************************************/
#include <cmath>
#include <cstdio>
#include "array_smoother.h"
#include "EST_cutils.h"

#define MAX_LEN             127

#define MODULE "array_smoother"

float median (int *counter, float valin, float valbuf[], int lmed, int mmed);
float hanning (int *counter, float valin, float valhan[], float win_coeff[], 
	       struct Ms_Op *ms);
void mk_window_coeffs (int length, float win_coeff[]);

struct Ms_Op *default_ms_op(struct Ms_Op *ms);

void array_smoother (float *p_array, int arraylen, struct Ms_Op *ms)
{
    int i, j, mid1, mid2 = 0, filler, nloops;
    int C1, C2 = 0, C3 = 0, C4 = 0, c1, c2, c3, c4;
    int delay, delx = 0, dely = 0;
    int in = 0, out = 0;
    float input, output;
    float *inarray;
    float xdel[2 * MAX_LEN - 2], ydel[2 * MAX_LEN - 2];
    float medbuf1[MAX_LEN], medbuf2[MAX_LEN];
    float hanbuf1[MAX_LEN], hanbuf2[MAX_LEN], win_coeffs[MAX_LEN];
    float medval1, medval2, hanval1, hanval2, zatn;

    inarray = new float[arraylen];
    for (i = 0; i < arraylen; ++i)
	inarray[i] = p_array[i];

    if (ms == NULL)
    { 
	ms = new Ms_Op;
	default_ms_op(ms);
    }

    mk_window_coeffs (ms->window_length, win_coeffs);
    /* determine the size and delay of each stage concerned */
    mid1 = ms->first_median / 2;
    C1 = delay = ms->first_median - 1;
    if (ms->apply_hanning) 	
    {
	C2 = ms->window_length - 1;
	delay = ms->first_median + ms->window_length - 2;
    }
    if (ms->smooth_double) {
	mid2 = ms->second_median / 2;
	C3 = ms->second_median - 1;
	if (!ms->apply_hanning) {
	    delx = ms->first_median;
	    dely = ms->second_median;
	}
	else {
	    C4 = ms->window_length - 1;
	    delx = ms->first_median + ms->window_length - 1;
	    dely = ms->second_median + ms->window_length - 1;
	}
	delay = delx + dely - 2;
    }
    /* prepare for smoothing */
    c1 = C1;
    c2 = C2;
    c3 = C3;
    c4 = C4;
    if (!ms->extrapolate) {
	/* pad with breakers at the beginning */
	for (i = 0; i < delay / 2; i++)
	    p_array[out++] = ms->breaker; 
	filler = 0;
	nloops = arraylen;
    }
    else {
	/* extrapolate by initialising filter with dummy breakers */
	filler = delay / 2;
	nloops = arraylen + delay;
    }
    /* smooth track element by track element */
    for (j = 0; j < nloops; j++) 
    {
	if (j < filler || j >= nloops - filler)
	    input = ms->breaker;
	else
	    input = inarray[in++];

	/* store input value if double smoothing */
	if (ms->smooth_double) {
	    for (i = delx - 1; i > 0; i--)
		xdel[i] = xdel[i - 1];
	    xdel[0] = input;
	}
	/* first median smoothing */

	medval1 = median (&c1, input, medbuf1, ms->first_median, mid1);

	if (c1 == -1) 
	{
	    output = medval1;
	    /* first hanning window (optional) */
	    if (ms->apply_hanning) 
	    {
		hanval1 = hanning (&c2, medval1, hanbuf1, win_coeffs, ms);
		if (c2 == -1)
		    output = hanval1;
		else
		    continue;
	    }
	    /* procedures for double smoothing (optional) */
	    if (ms->smooth_double) 
	    {
		/* compute rough component z(n) */
		if (output != ms->breaker && xdel[delx - 1] 
		    != ms->breaker)
		    zatn = xdel[delx - 1] - output;
		else
		    zatn = ms->breaker;
		/* store results of first smoothing */
		for (i = dely - 1; i > 0; i--)
		    ydel[i] = ydel[i - 1];
		ydel[0] = output;
		/* second median smoothing */
		medval2 = median (&c3, zatn, medbuf2, 
				  ms->second_median, mid2);
		if (c3 == -1) 
		{
		    output = medval2;
		    /* second hanning smoothing (optional) */
		    if (ms->apply_hanning) {
			hanval2 = hanning (&c4, medval2, hanbuf2, 
					   win_coeffs, ms);
			if (c4 == -1)
			    output = hanval2;
			else
			    continue;
		    }
		    if (output != ms->breaker && ydel[dely - 1] 
			!= ms->breaker)
			output += ydel[dely - 1];
		    else
			output = ms->breaker;
		}
		else
		    continue;
	    }
	    /* write filtered result */
	    p_array[out++] = output;
	}
    }
    if (!ms->extrapolate) 	/* pad with breakers at the end */
	for (i = 0; i < delay / 2; i++)
	    p_array[out++] = ms->breaker;

    delete inarray;
}

float median (int *counter, float valin, float valbuf[], int lmed, int mmed)
{
    int i, j;
    float tmp, filmed[MAX_LEN];

    for (i = lmed - 1; i > 0; i--)
	valbuf[i] = valbuf[i - 1];
    valbuf[0] = valin;

    if (*counter > 0) 
    {
	(*counter)--;
	return (0.0);
    }
    else 
    {
	*counter = -1;

	for (i = 0; i < lmed; i++)
	    filmed[i] = valbuf[i];

	for (j = lmed - 1; j > 0; j--)
	    for (i = 0; i < j; i++)
		if (filmed[i] > filmed[i + 1]) 
		{
		    tmp = filmed[i + 1];
		    filmed[i + 1] = filmed[i];
		    filmed[i] = tmp;
		}
	return (filmed[mmed]);
    }

}

#define TWO_PI 6.28318530717958647698

void mk_window_coeffs (int length, float win_coeff[])
{
    int i;
    double x;

    for (i = 0; i < length; i++) {
	x = TWO_PI * (i + 1.0) / (length + 1.0);
	win_coeff[i] = (1.0 - (float) cos (x)) / (length + 1.0);
    }

}

float hanning (int *counter, float valin, float valhan[], float win_coeff[], 
	       struct Ms_Op *par)
{
    int i, j, k = 0;
    float valout = 0.0, weight[MAX_LEN];

    for (i = par->window_length - 1; i > 0; i--)
	valhan[i] = valhan[i - 1];
    valhan[0] = valin;
    if (*counter > 0) {
	(*counter)--;
	return (0.0);
    }
    else {
	*counter = -1;
	for (i = 0; i < par->window_length; i++)
	    if (valhan[i] == par->breaker)
		k++;
	if (!k)
	    for (i = 0; i < par->window_length; i++)
		valout += valhan[i] * win_coeff[i];
	else if (k <= par->window_length / 2 && par->extrapolate) {
	    mk_window_coeffs (par->window_length - k, weight);
	    for (i = 0, j = 0; i < par->window_length; i++)
		if (valhan[i] != par->breaker)
		    valout += valhan[i] * weight[j++];
	}
	else
	    valout = par->breaker;
	return (valout);
    }

}

void initialise_parameters (struct Ms_Op *p_par)
{
    p_par->smooth_double = 0;
    p_par->apply_hanning = 0;
    p_par->extrapolate = 0;
    p_par->window_length = DEFAULT_WLEN;
    p_par->first_median = DEFAULT_MED_1;
    p_par->second_median = DEFAULT_MED_2;
    return;
}

struct Ms_Op *default_ms_op(struct Ms_Op *ms)
{
    ms->smooth_double = FALSE;
    ms->apply_hanning = TRUE;
    ms->extrapolate = TRUE;
    ms->first_median = 11;
    ms->second_median = 1; 
    ms->window_length = 7; 
    ms->breaker = -1.0;
    return (ms);
}
