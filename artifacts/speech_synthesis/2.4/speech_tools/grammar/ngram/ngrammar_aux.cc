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
/*                     Author :  Simon King & Alan W Black               */
/*                     Date   :  February 1997                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Auxiliary functions for EST_Ngram class                               */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include <cstring>
#include "EST_String.h"
#include "EST_Ngrammar.h"

static bool
ExponentialFit(EST_DVector &N, double &a, double &b, int first, int last)
{
  // fit the function  N(r) = e^a * r^b  where a,b are constants
  // to the points N(first)...N(last) inclusive
  // i.e. a straight line fit in the (natural) log domain
  // minimising the mean squared error (in log domain), is :
  
  // limitation : r must be an integer (it is the index of v)
  
  
  // default
  if (last == -1)
    last = N.n()-1;
  
  if (first < 0)
    {
      cerr << "ExponentialFit : first must be >= 0" << endl;
      return false;
    }
  
  if (last >= N.n()-1)
    {
      cerr << "ExponentialFit : last must be < N.n()-1 = " << N.n()-1 << endl;
    }
  
  if (first == last) 
    {
      a=log(N(first));
      b=0;
      return true;
    }
  
  double ElnNr=0.0,ElnNrlnr=0.0,
  Elnr=0.0,Elnr2=0.0,
  R=0.0;
  
  for(int r=first;r<=last;r++)
    {
      R += 1;
      if ( N(r) > 0)
	{
	  ElnNr += log( N(r) );
	  ElnNrlnr += log( N(r) ) * log( (double)r );
	}
      Elnr += log( (double)r );
      Elnr2 += log( (double)r ) * log( (double)r );
    }
  
  // and the answer is :
  b = ( (ElnNr*Elnr/R) - ElnNrlnr ) / ( (Elnr*Elnr/R) - Elnr2);
  a = (ElnNr - (b*Elnr) ) / R;
  
  return true;
}

static bool 
smooth_ExponentialFit(EST_DVector &N, int first, int last)
{
  double a=0.0,b=0.0;
  
  if (!ExponentialFit(N,a,b,first,last)) 
    {
      cerr << "smooth_ExponentialFit : ExponentialFit failed !" << endl;
      return false;
    }
  
  for(int r=first;r<=last;r++)
    N[r] = exp(a)* pow((double)r, b);
  
  return true;
}

void make_f_of_f(EST_BackoffNgrammarState *s,void *params)
{
    EST_Litem *k;
    double freq;
    EST_String name;

    EST_DVector *ff = (EST_DVector*)params;
    int max = ff->n();
    for (k=s->pdf_const().item_start();
	 !s->pdf_const().item_end(k);
	 k = s->pdf_const().item_next(k))
    {
	s->pdf_const().item_freq(k,name,freq);
	if(freq+0.5 < max)
	    (*ff)[(int)(freq+0.5)] += 1;
	  
    }


}

void get_max_f(EST_BackoffNgrammarState *s,void *params)
{
    EST_Litem *k;
    double freq;
    EST_String name;

    double *max = (double*)params;
    for (k=s->pdf_const().item_start();
	 !s->pdf_const().item_end(k);
	 k = s->pdf_const().item_next(k))
      {
	  s->pdf_const().item_freq(k,name,freq);
	  if(freq > *max)
	    *max = freq;
	  
      }
    
    
}

void map_f_of_f(EST_BackoffNgrammarState *s,void *params)
{
    EST_Litem *k;
    double freq;
    EST_String name;

    //cerr << "map_f_of_f : visited " << *s << endl;

    EST_DVector *map = (EST_DVector*)params;
    int max = map->n();
    for (k=s->pdf_const().item_start();
	 !s->pdf_const().item_end(k);
	 k = s->pdf_const().item_next(k))
      {
	  s->pdf_const().item_freq(k,name,freq);
	  if (freq+0.5 < max)
	    {
		double nfreq = (*map)((int)(freq+0.5));
		s->pdf().set_frequency(name,nfreq);

	    }
	  
      }
 
}

void zero_small_f(EST_BackoffNgrammarState *s,void *params)
{
    EST_Litem *k;
    double freq;
    EST_String name;

    double *min = (double*)params;
    for (k=s->pdf_const().item_start();
	 !s->pdf_const().item_end(k);
	 k = s->pdf_const().item_next(k))
      {
	  s->pdf_const().item_freq(k,name,freq);
	  if (freq < *min)
	  {
	      //cerr << "zeroing " << freq << " < " << *min << endl;
	      s->pdf().override_frequency(k,0.0);
	  }
      }
}

void frequency_of_frequencies(EST_DVector &ff, EST_Ngrammar &n,int this_order)
{
  int i,size;
  EST_Litem *k;
  double max=0.0;

  // if ff has zero size, do complete frequency of frequencies
  // otherwise just fill in frequencies from 1 to ff.n()-1

  bool complete = (bool)(ff.n() == 0);

  switch(n.representation())
    {

      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
	{
	    size = n.num_states();
	    if (complete)
	      {
		  // find highest frequency in EST_Ngram
		  for(i=0;i<size;i++)
		    if (n.p_states[i].pdf().samples() > max)
		      max = n.p_states[i].pdf().samples();
		  
		  ff.resize((int)(max+1.5));
		  ff.fill(0.0);
	      }

	    // Sum the frequencies
	    for(i=0;i<size;i++)
	      {
		  for (k=n.p_states[i].pdf().item_start();
		       !n.p_states[i].pdf().item_end(k);
		       k = n.p_states[i].pdf().item_next(k))
		    {
			EST_String name;
			double freq;
			n.p_states[i].pdf().item_freq(k,name,freq);
			ff[(int)(freq+0.5)] += 1;
		    }
	      }
	    
	    if (complete)
	      {
		  // fill in ff(0) - the number of unseen ngrams
		  
		  double total=0;
		  for (i=1;i<ff.n();i++)
		    total += ff(i);
		  
		  ff[0] = pow(float(n.get_vocab_length()),float(n.order())) - total;
	      }
	}
	break;
	

      case EST_Ngrammar::backoff:
	{
	    if (complete)
	      {
		  n.backoff_traverse(n.backoff_representation,
				     &get_max_f,(void*)(&max),
				     this_order-1);
		  ff.resize((int)(max+1.5));
	      }
	    
	    // backoff grammar has grammars of orders
	    // order, order-1, ..., 1
	    // and we treat each order independently

	    for (i=0;i<ff.n();i++)
		ff[i] = 0;

	    n.backoff_traverse(n.backoff_representation,
			       &make_f_of_f,(void*)(&ff),
			       this_order-1);
	    
	    if (complete)
	      {
		  // fill in ff(0) - the number of unseen ngrams		  
		  double total=0;
		  for (i=1;i<ff.n();i++)
		    total += ff(i);
		  ff[0] = pow(float(n.get_vocab_length()),float(this_order)) - total;



	      }
	}
	break;

      default:
	cerr << "unknown representation for EST_Ngrammar" << endl;
	break;
    }

}

void map_frequencies(EST_Ngrammar &n, const EST_DVector &map, const int this_order)
{
    int i;
    EST_Litem *k;

  switch(n.representation())
    {

      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
	{
	    int size = n.p_num_states;
	    
	    for(i=0;i<size;i++)
	      {
		  for (k=n.p_states[i].pdf().item_start();
		       !n.p_states[i].pdf().item_end(k);
		       k = n.p_states[i].pdf().item_next(k))
		    {
			EST_String name;
			double freq,nfreq;
			n.p_states[i].pdf().item_freq(k,name,freq);
			nfreq = map((int)(freq+0.5));
			n.p_states[i].pdf().set_frequency(name,nfreq);


		    }
	      }
	}
	break;

      case EST_Ngrammar::backoff:
	{

	    // backoff grammar has grammars of orders
	    // order, order-1, ..., 1
	    // and we treat each order independently
	    n.backoff_traverse(n.backoff_representation,
			       &map_f_of_f,(void*)(&map),
			       this_order-1);
	    
	}
	break;

      default:
	cerr << "unknown representation for EST_Ngrammar" << endl;
	break;

    }
}

static void 
adjusted_frequencies_BasicGoodTuring(EST_DVector &M,
				     const EST_DVector &N, 
				     int maxcount)
{
    // produce a map of frequencies, given a (smoothed) distribution
    // only map up to a frequency of maxcount; beyond that
    // map(r) == r
    
    if (maxcount > N.n()-2)
    {
	maxcount = N.n()-2;
	cerr << "adjusted_frequencies_BasicGoodTuring :";
	cerr << " maxcount is too big, reducing it to " << maxcount << endl;
    }
    
    M.resize(N.n());
    int r;
    for(r=0; r<=maxcount;r++)
    {
	// don't like this bit, but what can you do ?
	if( (N(r+1) == 0) || (N(r) == 0) )
	    M[r] = r;
	else
	    M[r] = (r + 1) * N(r+1) / N(r);
    }  
    // and do not map higher counts
    for(;r<N.n();r++)
	M[r]=r;
    
    return;
    
}

static void
smoothed_frequency_distribution_ExponentialFit(EST_DVector &N,int maxcount)
{
    if (maxcount > N.n()-2)
    {
	maxcount = N.n()-2;
	cerr << "smoothed_frequency_distribution_ExponentialFit :"
	    << " maxcount too big, reducing it to " << maxcount << endl;
    }
    // the idea to fit this particular fn was by Steve Isard
    // we ignore r=0 in doing the fit
    
    if (!smooth_ExponentialFit(N,1,maxcount+1))
	cerr << "smooth_ExponentialFit failed !" << endl;
    
    return;
}

bool
Good_Turing_smooth(EST_Ngrammar &ngrammar, int maxcount, int mincount)
{
    // works for any N
    // since it just remaps frequencies
    
    // frequencies above maxcount are not changed
    // for discounting -- see discount routine !
    
    
    if (ngrammar.entry_type() != EST_Ngrammar::frequencies)
    {
	cerr << "EST_Ngram: cannot Good-Turing smooth ngram:"  <<
	    " entries are not frequencies" << endl;
	return false;
    }
    
    switch(ngrammar.representation())
    {
	
      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
    {
	EST_DVector freqs,mapped_freqs;
	// grammar is of a single order - simple
	// Find frequency distribution
	frequency_of_frequencies(freqs,ngrammar,0);
	// smoothing should be optional - to do
	smoothed_frequency_distribution_ExponentialFit(freqs,maxcount-1);
	// Build map of frequencies
	adjusted_frequencies_BasicGoodTuring(mapped_freqs,freqs,maxcount);
	// Map all frequencies in grammar to Good Turing Smoothed values
	map_frequencies(ngrammar,mapped_freqs,0);
	
    }
    break;
    
  case EST_Ngrammar::backoff:
{
    
    cerr << "Smoothing of backed of grammars is not available!" << endl;
    return false;
    
    (void)mincount;
    
    /*
       // need to smooth for each order independently
       int i,o;
       double threshold;
       
       //for (o=1;o<=ngrammar.order();o++)
       for(o=ngrammar.order();o<=ngrammar.order();o++)
       {
       
       EST_DVector freqs,mapped_freqs;
       
       frequency_of_frequencies(freqs,ngrammar,o);
       
       cerr << "FREQ : " << freqs << endl;
       
       if(freqs.n() < 2)
       {
       // i.e. only unseen things - zero them all
       threshold = 2; // 1 ?
       if(o>1)
       ngrammar.backoff_traverse(ngrammar.backoff_representation,
       &zero_small_f,
       (void*)(&threshold),
       o-1);
       continue;
       }
       
       int max = maxcount;
       if(max > freqs.n() - 2)
       max = freqs.n() - 2;
       
       if(max > 2)
       // need at least 3 points
       // smoothing should be optional - to do
       smoothed_frequency_distribution_ExponentialFit(freqs,max);
       
       cerr << "SMOOTHED : " << freqs << endl;
       
       
       adjusted_frequencies_BasicGoodTuring(mapped_freqs,freqs,max);
       
       // initial map of frequencies modifies total counts
       // so pdfs are correct
       
       map_frequencies(ngrammar,mapped_freqs,o);
       
       
       cerr << "Map for " << o << " : ";
       for(i=0;i<max;i++)
       cerr << mapped_freqs(i) << " ";
       cerr << endl;
       
       cerr << "MAP : " << mapped_freqs << endl;
       
       // now go and zero the small frequencies
       // but not for unigram
       
       
       if(mincount < mapped_freqs.n())
       threshold = mapped_freqs(mincount);
       else
       threshold = mapped_freqs(mapped_freqs.n()-1) + 1; // i.e. zero everything
       
       //cerr << "min = " << threshold << endl;
       
       if(o>1)
       ngrammar.backoff_traverse(ngrammar.backoff_representation,
       &zero_small_f,
       (void*)(&threshold),
       o-1);
       */
    
}
break;

default:
cerr << "unknown representation for EST_Ngrammar" << endl;
break;

}

return true;

}


void
Good_Turing_discount(EST_Ngrammar &ngrammar, const int maxcount,
		     const double default_discount)
{
    
    if(ngrammar.representation() != EST_Ngrammar::backoff)
    {
	cerr << "Good_Turing_discount is not appropriate for non backoff grammar !"
	    << endl;
	return;
    }
    
    
    
    // method (for each grammar order):
    // produce Good-Turing smoothed frequency map
    // compute difference between unsmoothed map (0,1,2,3...)
    // and GT map  - this is the discount
    // we do not subtract the discount here since we need to
    // preserve the raw counts
    
    // discounting is applied up to frequency 'maxcount'
    // beyond which we do not discount
    
    // to do : zero low frequencies ???? (prune the tree)
    /*
       ngrammar.backoff_traverse(ngrammar.backoff_representation,
       &zero_small_f,
       (void*)(&threshold),
       o-1);
       */    
    
    // need to treat for each order independently
    int i,o;
    
    // no discounting for order 1 - YES !?!?!?
    for (o=1;o<=ngrammar.order();o++)
    {
	
	EST_DVector freqs,mapped_freqs;
	frequency_of_frequencies(freqs,ngrammar,o);
	
	int max = maxcount;
	if(max > freqs.n() - 2)
	    max = freqs.n() - 2;
	
	if(max > 2)
	{
	    // need at least 3 points
	    // smoothing should be optional - to do
	    
	    // April 97
	    // to overcome problems with N(x)=0, shift data points, fit, and shift back
	    
	    for(i=0;i<=max+1;i++)
		freqs[i] += 1;
	    
	    smoothed_frequency_distribution_ExponentialFit(freqs,max);
	    
	    for(i=0;i<=max+1;i++)
	    {
		freqs[i] -= 1;
		// possibly unnecesary check
		if ( freqs(i) < 0 )
		    freqs[i] = 0;
	    }
	}
	
	adjusted_frequencies_BasicGoodTuring(mapped_freqs,freqs,max);
	
	// and put it in the discount array
	ngrammar.backoff_discount[o-1].resize(freqs.n());
	for(i=(int)ngrammar.backoff_threshold;i<=max;i++)
	{
	    ngrammar.backoff_discount[o-1][i] =  (double)i - mapped_freqs(i);
	    
	    // sanity check
	    if( ngrammar.backoff_discount[o-1][i] < 0)
	    {
		ngrammar.backoff_discount[o-1][i] = 0;
	    }
	}
	
	for(;i<freqs.n();i++)
	    ngrammar.backoff_discount[o-1][i] = default_discount;

    }
}

VAL_REGISTER_CLASS(ngrammar,EST_Ngrammar)

