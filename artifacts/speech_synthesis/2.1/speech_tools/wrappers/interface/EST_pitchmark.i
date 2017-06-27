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
/*               Date :  25 Jan 2004                                     */
/* -------------------------------------------------------------------   */
/*   signal processing functions to do with pitchmarks/f0                */
/*                                                                       */
/*************************************************************************/

%module EST_pitchmark

%{
#include "sigpr/EST_pitchmark.h"
%}

%include "EST_typemaps.i"
%import "EST_Wave.i"
%import "EST_Track.i"
%import "EST_Features.i"

EST_Track pitchmark(EST_Wave &lx, EST_Features &op);


EST_Track pitchmark(EST_Wave &lx, int lx_lf, int lx_lo, int lx_hf, 
		    int lx_ho, int df_lf, int df_lo, int mo, int debug=0);


void neg_zero_cross_pick(EST_Wave &lx, EST_Track &pm);

void pm_fill(EST_Track &pm, float new_end, float max, float min, float def);
void pm_min_check(EST_Track &pm, float min);

void pm_to_f0(EST_Track &pm, EST_Track &f0);
void pm_to_f0(EST_Track &pm, EST_Track &fz, float shift);

