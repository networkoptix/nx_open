/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1999                            */
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
/*                     Author :  Alan W Black                            */
/*                     Date   :  February 1999                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* A rationalization of some of the general functions                    */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include <cstring>
#include "EST_String.h"
#include "EST_Token.h"
#include "EST_error.h"
#include "EST_Ngrammar.h"

static int get_next_window(EST_TokenStream &ts,
			   EST_StrVector &window,
			   const EST_String &input_format,
			   EST_Ngrammar &ngram)
{
    int i;
    if ((input_format == "sentence_per_line") ||
	(input_format == "sentence_per_file"))
    {
	EST_String t = ts.get().string();
	slide(window,-1);
	window[ngram.order()-1] = t;
	if (ngram.wordlist_index(t) == -1)
	    cerr << "EST_Ngrammar test: skipping bad word \"" <<
		t << "\"" << endl;
    }
    else if (input_format == "ngram_per_line")
    {
	for (i=0; i < ngram.order(); i++)
	{
	    EST_String t = ts.get().string();
	    window[i] = t;
	    if (ngram.wordlist_index(t) == -1)
		cerr << "EST_Ngrammar test: skipping bad word \"" <<
		    t << "\"" << endl;
	}
    }
    else
	EST_error("EST_Ngrammar test: unknown input format \"%s\"\n",
		  (const char *)input_format);

    // Sigh, you pull a little thread and it all falls down
    // For the time being can only deal in StrVectors rather than
    // IVectors
    for (i=0; i < ngram.order(); i++)
	if (ngram.wordlist_index(window(i)) == -1)
	    return FALSE;
    return TRUE;
}

bool test_stats(EST_Ngrammar &ngram, 
		const EST_String &filename,
		double &raw_entropy,
		double &count,
		double &entropy,
		double &perplexity,
		const EST_String &input_format,
		const EST_String &prev,
		const EST_String &prev_prev,
		const EST_String &last)
{
    // Apply an ngram to some data and report on its performance
    // Output entropy and test set perplexity
    // H = -1/Q . log P(wi | wi-1, wi-2, ... wi-n)
    // H_p = 2^H
    // Rabiner and Juang p450
    EST_TokenStream ts;
    double H,prob;
    int Q;
    EST_StrVector window(ngram.order());
    (void)last;

    if (filename == "-")
	ts.open(stdin,FALSE);
    else if (ts.open(filename) == -1)
	EST_error("EST_Ngrammar test: unable to open test file \"%s\"\n",
		  (const char *)filename);

    Q=0;
    H=0.0;
    ngram.fill_window_start(window,prev,prev_prev);

    while (!ts.eof() && 
	   (get_next_window(ts,window,input_format,ngram) == TRUE))
    {
	prob = ngram.probability(window);
	H += log(prob);
	Q++;
	if ((input_format == "sentence_per_line") && (ts.eoln()))
	    ngram.fill_window_start(window,prev,prev_prev);
    }

    count = Q;
    raw_entropy = -1 * H;
    entropy = -1 * (H/Q);
    perplexity = pow(2.0,entropy);

//    printf("count %g entropy %g perplexity %g\n",
//	   count,entropy,perplexity);

    return true;
}
