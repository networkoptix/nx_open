/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1998                            */
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
/*                       Author :  Alan W Black (and lots of books)      */
/*                       Date   :  January 1998                          */
/*-----------------------------------------------------------------------*/
/* Ordinary Least Squares/Linear regression                              */
/*                                                                       */
/*=======================================================================*/
#include <cmath>
#include "EST_multistats.h"
#include "EST_simplestats.h"

static void ols_load_selected_feats(const EST_FMatrix &X, 
				    const EST_IVector &included,
				    EST_FMatrix &Xl);
static int ols_stepwise_find_best(const EST_FMatrix &X,
				  const EST_FMatrix &Y,
				  EST_IVector &included,
				  EST_FMatrix &coeffs,
				  float &bscore,
				  int &best_feat,
				  const EST_FMatrix &Xtest,
				  const EST_FMatrix &Ytest,
                                  const EST_StrList &feat_names
                                  );

int ols(const EST_FMatrix &X,const EST_FMatrix &Y, EST_FMatrix &coeffs)
{
    // Ordinary least squares, X contains the samples with 1 (for intercept)
    // in column 0, Y contains the single values.
    EST_FMatrix Xplus;

    if (!pseudo_inverse(X,Xplus))
	return FALSE;

    multiply(Xplus,Y,coeffs);

    return TRUE;
}

int robust_ols(const EST_FMatrix &X,
	       const EST_FMatrix &Y, 
	       EST_FMatrix &coeffs)
{
    EST_IVector included;
    int i;

    included.resize(X.num_columns());
    for (i=0; i<included.length(); i++)
	 included.a_no_check(i) = TRUE;

    return robust_ols(X,Y,included,coeffs);
}

int robust_ols(const EST_FMatrix &X,
	       const EST_FMatrix &Y, 
	       EST_IVector &included,
	       EST_FMatrix &coeffs)
{
    // as with ols but if the pseudo inverse fails remove the offending
    // features and try again until it works, this can be costly but
    // its saves *you* from finding the singularity
    // This expands the output and puts weights of 0 for omitted features
    EST_FMatrix Xl;
    EST_FMatrix coeffsl;
    EST_FMatrix Xplus;
    int i,j,singularity=-1;

    if (X.num_rows() <= X.num_columns())
    {
	cerr << "OLS: less rows than columns, so cannot find solution."
	    << endl;
	return FALSE;
    }
    if (X.num_columns() != included.length())
    {
	cerr << "OLS: `included' list wrong size: internal error."
	    << endl;
	return FALSE;
    }

    while (TRUE)
    {
	ols_load_selected_feats(X,included,Xl);
	if (pseudo_inverse(Xl,Xplus,singularity))
	{
	    multiply(Xplus,Y,coeffsl);
	    break;
	}
	else
	{   // found a singularity so try again without that column
	    // remap singularity position back to X
	    int s;
	    for (s=i=0; i<singularity; i++)
	    {
		s++;
		while ((included(s) == FALSE) ||
                       (included(s) == OLS_IGNORE))
                       s++;
	    }
	    if (included(s) == FALSE)
	    {   // oops
		cerr << "OLS: found singularity twice, shouldn't happen" 
		    << endl;
		return FALSE;
	    }
	    else
	    {
		cerr << "OLS: omitting singularity in column " << s << endl;
		included[s] = FALSE;
	    }
	}
    }
	 
    // Map coefficients back, making coefficient 0 for singular cols
    coeffs.resize(X.num_columns(),1);
    for (j=i=0; i<X.num_columns(); i++)
	if (included(i))
	{
	    coeffs.a_no_check(i,0) = coeffsl(j,0);
	    j++;
	}
	else
	    coeffs.a_no_check(i,0) = 0.0;


    return TRUE;

}

static void ols_load_selected_feats(const EST_FMatrix &X, 
				    const EST_IVector &included,
				    EST_FMatrix &Xl)
{
    int i,j,k,width;

    for (width=i=0; i<included.length(); i++)
	if (included(i) == TRUE)
	    width++;

    Xl.resize(X.num_rows(),width);

    for (i=0; i<X.num_rows(); i++)
	for (k=j=0; j < X.num_columns(); j++)
	    if (included(j) == TRUE)
	    {
		Xl.a_no_check(i,k) = X.a_no_check(i,j);
		k++;
	    }
	 
}

int ols_apply(const EST_FMatrix &samples,
	      const EST_FMatrix &coeffs,
	      EST_FMatrix &res)
{
    // Apply coefficients to samples for res.

    if (samples.num_columns() != coeffs.num_rows())
	return FALSE;

    multiply(samples,coeffs,res);

    return TRUE;
}

int stepwise_ols(const EST_FMatrix &X,
		 const EST_FMatrix &Y,
		 const EST_StrList &feat_names,
		 float limit,
		 EST_FMatrix &coeffs,
		 const EST_FMatrix &Xtest,
		 const EST_FMatrix &Ytest,
                 EST_IVector &included)
{
    // Find the features that contribute to the correlation using a
    // a greedy algorithm

    EST_FMatrix coeffsl;
    float best_score=0.0,bscore;
    int i,best_feat;
    int nf=1;  // for nice printing of progress

    for (i=1; i < X.num_columns(); i++)
    {
	if (!ols_stepwise_find_best(X,Y,included,coeffsl,
				    bscore,best_feat,Xtest,Ytest,
                                    feat_names))
	{
	    cerr << "OLS: stepwise failed" << endl;
	    return FALSE;
	}
	if ((bscore - (bscore * (limit/100))) <= best_score)
	    break;
	else
	{
	    best_score = bscore;
	    coeffs = coeffsl;
	    included[best_feat] = TRUE;
	    printf("FEATURE %d %s: %2.4f\n",
		   nf,
		   (const char *)feat_names.nth(best_feat),
		   best_score);
	    fflush(stdout);
	    nf++;
	}
    }

    return TRUE;
}

static int ols_stepwise_find_best(const EST_FMatrix &X,
				  const EST_FMatrix &Y,
				  EST_IVector &included,
				  EST_FMatrix &coeffs,
				  float &bscore,
				  int &best_feat,
				  const EST_FMatrix &Xtest,
				  const EST_FMatrix &Ytest,
                                  const EST_StrList &feat_names
                                  )
{
    EST_FMatrix coeffsl;
    bscore = 0;
    best_feat = -1;
    int i;

    for (i=0; i < included.length(); i++)
    {
	if (included.a_no_check(i) == FALSE)
	{
	    float cor, rmse;
	    EST_FMatrix pred;
	    included.a_no_check(i) = TRUE;
	    if (!robust_ols(X,Y,included,coeffsl))
		return FALSE;  // failed for some reason
	    ols_apply(Xtest,coeffsl,pred);
            ols_test(Ytest,pred,cor,rmse);
            printf("tested %d %s %f best %f\n",
                   i,(const char *)feat_names.nth(i),cor,bscore);
	    if (fabs(cor) > bscore)
	    {
		bscore = fabs(cor);
		coeffs = coeffsl;
		best_feat = i;
	    }
	    included.a_no_check(i) = FALSE;
	}
    }

    return TRUE;
}

int ols_test(const EST_FMatrix &real,
	     const EST_FMatrix &predicted,
	     float &correlation,
	     float &rmse)
{
    // Others probably want this function too
    // return correlation and RMSE for col 0 in real and predicted
    int i;
    float p,r;
    EST_SuffStats x,y,xx,yy,xy,se,e;
    double error;
    double v1,v2,v3;

    if (real.num_rows() != predicted.num_rows())
	return FALSE;  // can't do this

    for (i=0; i < real.num_rows(); i++)
    {
	r = real(i,0);
	p = predicted(i,0);
	x += p;
	y += r;
	error = p-r;
	se += error*error;
	e += fabs(error);
	xx += p*p;
	yy += r*r;
	xy += p*r;
    }

    rmse = sqrt(se.mean());

    v1 = xx.mean()-(x.mean()*x.mean());
    v2 = yy.mean()-(y.mean()*y.mean());

    v3 = v1*v2;

    if (v3 <= 0)
    {   // happens when there's very little variation in x
	correlation = 0;
	rmse = se.mean();
	return FALSE;
    }
    // Pearson's product moment correlation coefficient
    correlation = (xy.mean() - (x.mean()*y.mean()))/ sqrt(v3);

    // I hate to have to do this but it is necessary.
    // When the the variation of X is very small v1*v2 approaches
    // 0 (the negative and equals case is caught above) but that
    // may not be enough when v1 or v2 are very small but positive.
    // So I catch it here.  If I knew more math I'd be able to describe
    // this better but the code would remain the same.
    if ((correlation <= 1.0) && (correlation >= -1.0))
	return TRUE;
    else
    {
	correlation = 0;
	return FALSE;
    }
}
