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
/*                  Author :  Paul Taylor & Simon King & Alan W Black    */
/*                  Date   :  April 1997                                 */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Yet another method for smoothing an ngram                             */
/*                                                                       */
/* Good_Turing smooth n-grammar so there are no zero occurrences         */
/* For each ngram                                                        */
/*    if |n-1gram| < threshold                                           */
/*        F(ngram) = !n-1gram|*(P(n-1gram))                              */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include <cstring>
#include <cmath>
#include <climits>
#include <cfloat>
#include "EST_Ngrammar.h"

static double fs_find_backoff_prob(EST_Ngrammar *backoff_ngrams,
				       int order,const EST_StrVector words,
				       int smooth_thresh);

void Ngram_freqsmooth(EST_Ngrammar &ngram,int smooth_thresh1,
		       int smooth_thresh2)
{
    //  To assign reasonable frequencies for all ngrams in grammar
    EST_Ngrammar *backoff_ngrams;
    backoff_ngrams = new EST_Ngrammar[ngram.order()-1];

    Good_Turing_smooth(ngram,smooth_thresh1,0);

    fs_build_backoff_ngrams(backoff_ngrams,ngram);

    fs_backoff_smooth(backoff_ngrams,ngram,smooth_thresh2);

    delete [] backoff_ngrams;

}

void fs_build_backoff_ngrams(EST_Ngrammar *backoff_ngrams,
				 EST_Ngrammar &ngram)
{
    // Build all the backoff grammars back to uni-grams
    int i,j,l;
    EST_Litem *k;

    for (i=0; i < ngram.order()-1; i++)
	backoff_ngrams[i].init(i+1,EST_Ngrammar::dense,
			       *ngram.vocab,*ngram.pred_vocab);

    for (i=0; i < ngram.num_states(); i++)
    {
	const EST_StrVector words = ngram.make_ngram_from_index(i);

	for (k=ngram.p_states[i].pdf().item_start();
	     !ngram.p_states[i].pdf().item_end(k);
	     k = ngram.p_states[i].pdf().item_next(k))
	{
	    double freq;
	    EST_String name;
	    ngram.p_states[i].pdf().item_freq(k,name,freq);
	    // Build all the sub-ngrams and accumulate them
	    for (j=0; j < ngram.order()-1; j++)
	    {
		EST_StrVector nnn(j+1);
		nnn[j] = name;
		for (l=0; l < j; l++)
		    nnn[l] = words(ngram.order()-1-j);
		backoff_ngrams[j].accumulate(nnn,freq);
	    }
	}
    }

}

int fs_backoff_smooth(EST_Ngrammar *backoff_ngrams,
			 EST_Ngrammar &ngram, int smooth_thresh)
{
    // For all ngrams which are too infrequent, adjust their
    // frequencies based on their backoff probabilities
    int i;
    EST_Litem *j;
    double occurs;
    double backoff_prob;

    if (ngram.representation() != EST_Ngrammar::dense)
    {
	cerr << "Ngrammar: can only ptsmooth dense ngrammars" << endl;
	return FALSE;
    }
    else
    {
	for (i=0; i < ngram.num_states(); i++)
	{
	    if (ngram.p_states[i].pdf().samples() < smooth_thresh)
	    {
		EST_DiscreteProbDistribution &pdf = ngram.p_states[i].pdf();
		occurs = ngram.p_states[i].pdf().samples();
		EST_StrVector words = ngram.make_ngram_from_index(i);
		words.resize(words.n()+1);
		
		for (j=pdf.item_start();
		     ! pdf.item_end(j);
		     j = pdf.item_next(j))
		{
		    EST_String name;
		    double freq;
		    pdf.item_freq(j,name,freq);
		    words[words.n()-1] = name;
		    backoff_prob = 
			fs_find_backoff_prob(backoff_ngrams,
					     ngram.order()-1,
					     words,
					     smooth_thresh);
		    pdf.set_frequency(j,occurs*backoff_prob);
		}
	    }
	}
    }

    return TRUE;
}

static double fs_find_backoff_prob(EST_Ngrammar *backoff_ngrams,
				   int order,const EST_StrVector words,
				   int smooth_thresh)
{
    // Find the backoff prob for n-1gram
    EST_StrVector nnn;
    int i;

    if (order == 0)
	return TINY_FREQ;   // ultimate floor

    nnn.resize(order);
    
    for(i=0; i<order; i++)
	nnn[order-1-i] = words(words.n()-1-i);

    if (backoff_ngrams[order-1].frequency(nnn) < smooth_thresh)
	return fs_find_backoff_prob(backoff_ngrams,
				       order-1,words,smooth_thresh);
    else
	return backoff_ngrams[order-1].probability(nnn);
}
