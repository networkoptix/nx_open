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
/*             Author :  Alan W Black                                    */
/*             Date   :  October 1997                                    */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Implementation of an inside-outside reestimation procedure for        */
/* building a stochastic CFG seeded with a bracket corpus.               */
/* Based on "Inside-Outside Reestimation from partially bracketed        */
/* corpora", F Pereira and Y. Schabes. pp 128-135, 30th ACL, Newark,     */
/* Delaware 1992.                                                        */
/*                                                                       */
/* This should really be done in the log domain.  Addition in the log    */
/* domain can be done with a formula in Huang, Ariki and Jack            */
/*                         (log(a)-log(b))                               */
/*    log(a+b) = log(1 + e                ) + log(b)                     */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include "EST_SCFG_Chart.h"
#include "EST_simplestats.h"
#include "EST_math.h"
#include "EST_TVector.h"

static const EST_bracketed_string def_val_s;
static EST_bracketed_string error_return_s;
template <> const EST_bracketed_string *EST_TVector<EST_bracketed_string>::def_val=&def_val_s;
template <> EST_bracketed_string *EST_TVector<EST_bracketed_string>::error_return=&error_return_s;


#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_TVector.cc"

template class EST_TVector<EST_bracketed_string>;
#endif

void set_corpus(EST_Bcorpus &b, LISP examples)
{
    LISP e;
    int i;

    b.resize(siod_llength(examples));

    for (i=0,e=examples; e != NIL; e=cdr(e),i++)
	b.a_no_check(i).set_bracketed_string(car(e));
}

void EST_bracketed_string::init()
{
    bs = NIL; 
    gc_protect(&bs); 
    symbols = 0;
    valid_spans = 0;
    p_length = 0;
}

EST_bracketed_string::EST_bracketed_string()
{
    init();
}

EST_bracketed_string::EST_bracketed_string(LISP string)
{ 
    init();

    set_bracketed_string(string); 
}

EST_bracketed_string::~EST_bracketed_string()
{ 
    int i;
    bs=NIL;
    gc_unprotect(&bs); 
    delete [] symbols;
    for (i=0; i < p_length; i++)
	delete [] valid_spans[i];
    delete [] valid_spans;
}

void EST_bracketed_string::set_bracketed_string(LISP string)
{
    
    bs=NIL;
    delete [] symbols;

    p_length = find_num_nodes(string);
    symbols = new LISP[p_length];

    set_leaf_indices(string,0,symbols);

    bs = string;

    int i,j;
    valid_spans = new int*[length()];
    for (i=0; i < length(); i++)
    {
	valid_spans[i] = new int[length()+1];
	for (j=i+1; j <= length(); j++)
	    valid_spans[i][j] = 0;
    }
    
    // fill in valid table 
    if (p_length > 0)
	find_valid(0,bs);

}

int EST_bracketed_string::find_num_nodes(LISP string)
{
    // This wont could nil as an atom
    if (string == NIL)
	return 0;
    else if (CONSP(string))
	return find_num_nodes(car(string))+
	    find_num_nodes(cdr(string));
    else
	return 1;
}

int  EST_bracketed_string::set_leaf_indices(LISP string,int i,LISP *syms)
{
    if (string == NIL)
	return i;
    else if (!CONSP(car(string)))
    {
	syms[i] = string;
	return set_leaf_indices(cdr(string),i+1,syms);
    }
    else  // car is a tree
    {
	return set_leaf_indices(cdr(string),
				set_leaf_indices(car(string),i,syms),
				syms);
    }
}

void EST_bracketed_string::find_valid(int s,LISP t) const
{
    LISP l;
    int c;

    if (consp(t))
    {
	for (c=s,l=t; l != NIL; l=cdr(l))
	{
	    c += num_leafs(car(l));
	    valid_spans[s][c] = 1;
	}
	find_valid(s,car(t));
	find_valid(s+num_leafs(car(t)),cdr(t));
    }
}

int EST_bracketed_string::num_leafs(LISP t) const
{
    if (t == NIL)
	return 0;
    else if (!consp(t))
	return 1;
    else 
	return num_leafs(car(t)) + num_leafs(cdr(t));
}

EST_SCFG_traintest::EST_SCFG_traintest(void) : EST_SCFG() 
{
    inside = 0;
    outside = 0;
    n.resize(0);
    d.resize(0);
}

EST_SCFG_traintest::~EST_SCFG_traintest(void)
{
    
}

void EST_SCFG_traintest::load_corpus(const EST_String &filename)
{
    set_corpus(corpus,vload(filename,1));
}

// From the formula in the paper
double EST_SCFG_traintest::f_I_cal(int c, int p, int i, int k)
{
    // Find Inside probability
    double res;

    if (i == k-1)
    {
	res = prob_U(p,terminal(corpus.a_no_check(c).symbol_at(i)));
//	printf("prob_U p %s (%d) %d m %s (%d) res %g\n",
//	       (const char *)nonterminal(p),p,
//	       i,
//	       (const char *)corpus.a_no_check(c).symbol_at(i),
//	       terminal(corpus.a_no_check(c).symbol_at(i)),
//	       res);
    }
    else if (corpus.a_no_check(c).valid(i,k) == TRUE)
    {
	int j;
	double s=0;
	int q,r;
	
	for (q = 0; q < num_nonterminals(); q++)
	    for (r = 0; r < num_nonterminals(); r++)
	    {
		double pBpqr = prob_B(p,q,r);
		if (pBpqr > 0)
		    for (j=i+1; j < k; j++)
		    {
			double in = f_I(c,q,i,j);
			if (in > 0)
			    s += pBpqr * in * f_I(c,r,j,k);
		    }
	    }
	res = s;
    }
    else
	res = 0.0;

    inside[p][i][k] = res;

//    printf("f_I p %s i %d k %d res %g\n",
//	   (const char *)nonterminal(p),i,k,res);

    return res;
}

double EST_SCFG_traintest::f_O_cal(int c, int p, int i, int k)
{
    // Find Outside probability
    double res;

    if ((i == 0) && (k == corpus.a_no_check(c).length()))
    {
	if (p == distinguished_symbol()) // distinguished non-terminal
	    res = 1.0;
	else
	    res = 0.0;
    }
    else if (corpus.a_no_check(c).valid(i,k) == TRUE)
    {
	double s1=0.0;
	double s2,s3;
	double pBqrp,pBqpr;
	int j;
	int q,r;

	for (q = 0; q < num_nonterminals(); q++)
	    for (r = 0; r < num_nonterminals(); r++)
	    {
		pBqrp = prob_B(q,r,p);
		s2 = s3 = 0.0;
		if (pBqrp > 0)
		{
		    for (j=0;j < i; j++)
		    {
			double out = f_O(c,q,j,k);
			if (out > 0)
			    s2 += out * f_I(c,r,j,i);
		    }
		    s2 *= pBqrp;
		}
		pBqpr = prob_B(q,p,r);
		if (pBqpr > 0)
		{
		    for (j=k+1;j <= corpus.a_no_check(c).length(); j++)
		    {
			double out = f_O(c,q,i,j);
			if (out > 0)
			    s3 += out * f_I(c,r,k,j);
		    }
		    s3 *= pBqpr;
		}
		s1 += s2 + s3;
	    }
	res = s1;
    }
    else  // not a valid bracketing
	res = 0.0;

    outside[p][i][k] = res;

    return res;
}

void EST_SCFG_traintest::reestimate_rule_prob_B(int c, int ri, int p, int q, int r)
{
    // Re-estimate probability for binary rules
    int i,j,k;
    double n2=0;
    
    double pBpqr = prob_B(p,q,r);

    if (pBpqr > 0)
    {
	for (i=0; i <= corpus.a_no_check(c).length()-2; i++)
	    for (j=i+1; j <= corpus.a_no_check(c).length()-1; j++)
	    {
		double d1 = f_I(c,q,i,j);
		if (d1 == 0) continue;
		for (k=j+1; k <= corpus.a_no_check(c).length(); k++)
		{
		    double d2 = f_I(c,r,j,k);
		    if (d2 == 0) continue;
		    double d3 = f_O(c,p,i,k);
		    if (d3 == 0) continue;
		    n2 += d1 * d2 * d3;
		}
	    }
	n2 *= pBpqr;
    }
    // f_P(c) is probably redundant
    double fp = f_P(c);
    double n1,d1;
    n1 = n2 / fp;
    if (fp == 0) n1=0;

    d1 = f_P(c,p) / fp;
    if (fp == 0) d1=0;
    //	printf("n1 %f d1 %f n2 %f fp %f\n",n1,d1,n2,fp);
    n[ri] += n1;
    d[ri] += d1;

}

void EST_SCFG_traintest::reestimate_rule_prob_U(int c,int ri, int p, int m)
{
    // Re-estimate probability for unary rules
    int i;

//    printf("reestimate_rule_prob_U: %f p %s m %s\n",
//	   prob_U(ip,im),
//	   (const char *)p,
//	   (const char *)m);

    double n2=0;
	
    for (i=1; i < corpus.a_no_check(c).length(); i++)
	if (m == terminal(corpus.a_no_check(c).symbol_at(i-1)))
	    n2 += prob_U(p,m) * f_O(c,p,i-1,i);

    double fP = f_P(c);
    if (fP != 0)
    {
	n[ri] += n2 / fP;
	d[ri] += f_P(c,p) / fP;
    }
}

double EST_SCFG_traintest::f_P(int c)
{
    return f_I(c,distinguished_symbol(),0,corpus.a_no_check(c).length());
}

double EST_SCFG_traintest::f_P(int c,int p)
{
    int i,j;
    double db=0;

    for (i=0; i < corpus.a_no_check(c).length(); i++)
	for (j=i+1; j <= corpus.a_no_check(c).length(); j++)
	{
	    double d1 = f_O(c,p,i,j);
	    if (d1 == 0) continue;
	    db += f_I(c,p,i,j)*d1;
	}

    return db;
}

void EST_SCFG_traintest::reestimate_grammar_probs(int passes,
					int startpass,
					int checkpoint,
					int spread,
					const EST_String &outfile)
{
    // Iterate over the corpus cummulating factors for each rules
    // This reduces the space requirements and recalculations of
    // values for each sentences.  
    // Repeat training passes to number specified
    int pass = 0;
    double zero=0;
    double se;
    int ri,c;

    n.resize(rules.length());
    d.resize(rules.length());

    for (pass = startpass; pass < passes; pass++)
    {
	EST_Litem *r;
	double mC, lPc;

	d.fill(zero);
	n.fill(zero);
	set_rule_prob_cache();

	for (mC=0.0,lPc=0.0,c=0; c < corpus.length(); c++)
	{
	    // For skipping some sentences to speed up convergence
	    if ((spread > 0) && (((c+(pass*spread))%100) >= spread))
		continue;
	    printf(" %d",c); fflush(stdout);
	    if (corpus.a_no_check(c).length() == 0) continue;
	    init_io_cache(c,num_nonterminals());
	    for (ri=0,r=rules.head(); r != 0; r=r->next(),ri++)
	    {
		if (rules(r).type() == est_scfg_binary_rule)
		    reestimate_rule_prob_B(c,ri,
					   rules(r).mother(),
					   rules(r).daughter1(),
					   rules(r).daughter2());
		else
		    reestimate_rule_prob_U(c,
					   ri,
					   rules(r).mother(),
					   rules(r).daughter1());
	    }
	    lPc += safe_log(f_P(c));
	    mC += corpus.a_no_check(c).length();
	    clear_io_cache(c);
	}
	printf("\n");

	for (se=0.0,ri=0,r=rules.head(); r != 0; r=r->next(),ri++)
	{
	    double n_prob = n[ri]/d[ri];
	    if (d[ri] == 0)
		n_prob = 0;
	    se += (n_prob-rules(r).prob())*(n_prob-rules(r).prob());
	    rules(r).set_prob(n_prob);
	}
	printf("pass %d cross entropy %g RMSE %f %f %d\n",
	       pass,-(lPc/mC),sqrt(se/rules.length()),
	       se,rules.length());
	
	if (checkpoint != -1) 
	{
	    if ((pass % checkpoint) == checkpoint-1)
	    {
		char cp[20];
		sprintf(cp,".%03d",pass);
		save(outfile+cp);
		user_gc(NIL);  // just to keep things neat
	    }
	}

    }
}

void EST_SCFG_traintest::train_inout(int passes,
				     int startpass,
				     int checkpoint,
				     int spread,
				     const EST_String &outfile)
{
    // Train a Stochastic CFG using the inside outside algorithm

    reestimate_grammar_probs(passes, startpass, checkpoint, 
			     spread, outfile);
}

void EST_SCFG_traintest::init_io_cache(int c,int nt)
{
    // Build an array to cache the in/out values
    int i,j,k;
    int mc = corpus.a_no_check(c).length()+1;
    
    inside = new double**[nt];
    outside = new double**[nt];
    for (i=0; i < nt; i++)
    {
	inside[i] = new double*[mc];
	outside[i] = new double*[mc];
	for (j=0; j < mc; j++)
	{
	    inside[i][j] = new double[mc];
	    outside[i][j] = new double[mc];
	    for (k=0; k < mc; k++)
	    {
		inside[i][j][k] = -1;
		outside[i][j][k] = -1;
	    }
	}
    }
}
	
void EST_SCFG_traintest::clear_io_cache(int c)
{
    int mc = corpus.a_no_check(c).length()+1;
    int i,j;

    if (inside == 0)
	return;

    for (i=0; i < num_nonterminals(); i++)
    {
	for (j=0; j < mc; j++)
	{
	    delete [] inside[i][j];
	    delete [] outside[i][j];
	}
	delete [] inside[i];
	delete [] outside[i];
    }

    delete [] inside;
    delete [] outside;

    inside = 0;
    outside = 0;
}

double EST_SCFG_traintest::cross_entropy()
{
    double lPc=0,mC=0;
    int c;

    for (c=0; c < corpus.length(); c++)
    {
	lPc += log(f_P(c));
	mC += corpus.a_no_check(c).length();
    }

    return -(lPc/mC);
}

void EST_SCFG_traintest::test_corpus()
{
    // Test corpus against current grammar.
    double mC,lPc;
    int c,i;
    int failed=0;
    double fP;

    // Lets try simply finding the cross entropy
    n.resize(rules.length());
    d.resize(rules.length());
    for (i=0; i < rules.length(); i++)
	d[i] = n[i] = 0.0;

    for (mC=0.0,lPc=0.0,c=0; c < corpus.length(); c++)
    {
	if (corpus.length() > 50)
	{
	    printf(" %d",c); 
	    fflush(stdout);
	}
	init_io_cache(c,num_nonterminals());
	fP = f_P(c);
	if (fP == 0)
	    failed++;
	else
	{
	    lPc += safe_log(fP);
	    mC += corpus.a_no_check(c).length();
	}
	clear_io_cache(c);
    }
    if (corpus.length() > 50)
	printf("\n");

    cout << "cross entropy " << -(lPc/mC) << " (" << failed << " failed out of " <<
	corpus.length() << " sentences )" << endl;

}

