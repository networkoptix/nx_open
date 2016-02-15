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
/*************************************************************************/
/*                                                                       */
/* The above copyright was given by Paul Bagshaw, he retains             */
/* his original rights                                                   */
/*                                                                       */
/*************************************************************************/
#ifndef array_smoother_INCLUDED
#define array_smoother_INCLUDED

#define DEFAULT_DOUBLE      0
#define DEFAULT_HANNING     1
#define DEFAULT_EXTRAPOLATE 1
#define DEFAULT_MED_1       3
#define DEFAULT_MED_2       3
#define DEFAULT_WLEN        3
#define DEFAULT_BREAKER     -9999.0


/*typedef struct {
  short smooth_double, apply_hanning;
  short extrapolate;
  int first_median, second_median, window_length;
  float breaker;
} SETTINGS_;

*/

struct  Ms_Op {			/* median smoother operations */
  int smooth_double;
  int apply_hanning;
  int extrapolate;
  int first_median;
  int second_median;
  int window_length;
  int interp;
  float breaker;
};

void array_smoother (float *p_array, int arraylen, struct Ms_Op *ms);


#endif
