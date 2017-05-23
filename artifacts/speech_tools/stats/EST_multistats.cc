/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                      Copyright (c) 1995,1996                          */
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
/*                       Author :  Paul Taylor                           */
/*                       Date   :  July 1995                             */
/*-----------------------------------------------------------------------*/
/*                Basic Multivariate Statistical Functions               */
/*                                                                       */
/*=======================================================================*/
#include <cmath>
#include "EST_multistats.h"

float mean(EST_FVector &v)
{
  int i;
  float u = 0.0;

  for (i = 0; i < v.n(); ++i)
    u += v(i);

  return u / v.n();
}

float sum(EST_FVector &v)
{
  int i;
  float u = 0.0;

  for (i = 0; i < v.n(); ++i)
    u += v(i);

  return u;
}

EST_FVector mean(EST_FMatrix &m)
{
    EST_FVector u(m.num_columns());
    int i, j;

    for (i = 0; i < m.num_columns(); ++i)
    {
	u[i] = 0.0;
	for (j = 0; j < m.num_rows(); ++j)
	    u[i] += m(j, i);
	u[i] /= m.num_rows();
    }

    return u;
}

EST_FVector sample_variance(EST_FMatrix &m)
{
    EST_FVector v(m.num_columns());
    EST_FVector u(m.num_columns());
    int i, j;
  
    u = mean(m);

    for (j = 0; j < m.num_columns(); ++j)
    {
	v[j] = 0.0;
	for (i = 0; i < m.num_rows(); ++i)
	  v[j] += pow(m(i, j) - u(j), float(2.0));
	v[j] /= m.num_rows() - 1; // sample variance
    }
    
    return v;
}

EST_FVector sample_stdev(EST_FMatrix &m)
{
    EST_FVector v;
    v = sample_variance(m);
    int j;
    
    for (j = 0; j < v.n(); ++j)
	v[j] = sqrt(v(j));
    
    return v;
}

EST_FMatrix sample_covariance(EST_FMatrix &m)
{
    EST_FVector u(m.num_columns());
    EST_FMatrix c(m.num_columns(), m.num_columns());
    int i, j, k;
    
    u = mean(m);
    
    for (j = 0; j < m.num_columns(); ++j)
	for (k = 0; k < m.num_columns(); ++k)
	{
	    c(j, k) = 0.0;
	    for (i = 0; i < m.num_rows(); ++i)
		c(j, k) += (m(i, j) - u(j)) * (m(i, k) - u(k));
	    c(j, k) /= m.num_rows();
	}
    
    
    return c;
}

EST_FMatrix sample_correlation(EST_FMatrix &m)
{
    EST_FMatrix r(m.num_columns(), m.num_columns());
    
    EST_FVector s = sample_stdev(m);
    EST_FMatrix c = sample_covariance(m);
    
    int j, k;
    
    for (j = 0; j < m.num_columns(); ++j)
	for (k = 0; k < m.num_columns(); ++k)
	    r(j, k) = c(j, k)/(s(j) * s(k));
    
    return r;
}

EST_FMatrix euclidean_distance(EST_FMatrix &m)
{
    // nowt yet
    return m;
}

// normalise matrix m, by subtracting  vector "sub" from each column and
// then dividing each column by vector div;

EST_FMatrix normalise(EST_FMatrix &m, EST_FVector &sub, EST_FVector &div)
{
    int i, j;
    EST_FMatrix z(m.num_rows(), m.num_columns());
    
    for (j = 0; j < m.num_columns(); ++j)
	for (i = 0; i < m.num_rows(); ++i)
	    z(i, j) = (m(i, j) - sub(j)) / div(j);
    
    return z;
}

// calculate penrose distance for matrix of means gu of p variables
// (columns) in g populations (columns), and variance gv.

EST_FMatrix penrose_distance(EST_FMatrix &gu, EST_FVector &gv)
{
    int i, j, k;
    int p = gu.num_columns();
    int n = gu.num_rows();
    EST_FMatrix P(n, n);
    
    cout << "pop mean " << gu;
    
    for (i = 0; i < n; ++i)
	for (j = 0; j < n; ++j)
	{
	    P(i, j) = 0.0;
	    for (k = 0; k < p; ++k)
	      P(i, j) += pow(gu(i, k) - gu(j, k), float(2.0)) / gv(k);
	    P(i, j) /= p;
	}
    return P;
}

float single_mahal(EST_FMatrix &ui, EST_FMatrix &uj, EST_FMatrix &invv)
{
    float e;
    EST_FMatrix a, b,c,d;
    
    a = ui - uj;
    transpose(a,b);
    multiply(b,invv,c);
    multiply(c,a,d);
    e = d(0,0);
    return e;
}

EST_FMatrix mahalanobis_distance(EST_FMatrix &gu, EST_FMatrix &covar)
{
    int i, j;
    int n = gu.num_rows();
    EST_FMatrix P(n, n), invv, ui, uj;
    
    inverse(covar,invv);	// should be inverse!!
    
    for (i = 0; i < n; ++i)
	for (j = 0; j < n; ++j)
	{
	    transpose(row(gu, i),ui);
	    transpose(row(gu, j),uj);
	    P(i, j) = single_mahal(ui, uj, invv);
	}
    
    return P;
}

float simple_penrose(EST_FVector &ui, EST_FVector &uj, EST_FVector &v)
{
    int k;
    int n = uj.n();
    float P = 0.0;
    
    for (k = 0; k < n; ++k)
      P += pow(ui(k) - uj(k), float(2.0)) / v(k);
    P /= n;
    
    return P;
}

EST_FMatrix population_mean(EST_FMatrix *in, int num_pop)
{
    int i, k;
    EST_FMatrix pmean(num_pop, in[0].num_columns());
    EST_FVector u(in[0].num_columns());
    
    for (i = 0; i < num_pop; ++i)
    {
	u = mean(in[i]);
	for (k =0; k < in[i].num_columns(); ++k)
	    pmean(i, k) = u(k);
    }
    return pmean;
}

EST_FMatrix add_populations(EST_FMatrix *in, int num_pop)
{
    int i, j, k, l, n;
    
    l = 0;
    for (i = 0; i < num_pop; ++i)
	l += in[i].num_rows();
    n = in[0].num_columns();
    
    EST_FMatrix msum(l, n);
    
    for (k = l = 0; k < num_pop; ++k)
	for (j =0; j < n; ++j, ++l)
	    for (i = 0; i < in[i].num_rows(); ++i)
		msum(l, j) = in[k](i, j);
    
    return msum;
}

