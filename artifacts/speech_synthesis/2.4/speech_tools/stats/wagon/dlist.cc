/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                      Copyright (c) 1996,1997                          */
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
/*                     Date   :  February 1997                           */
/*-----------------------------------------------------------------------*/
/*  Decision lists                                                       */
/*  These are like a right branching decision tree, yes answers are not  */
/*  partitioned any further.  These are used for the Yarowsky-style      */
/*  homograph disambiguators                                             */
/*                                                                       */
/*  Yarowsky, D. "Homograph Disambiguation in Text-to_Speech Synthesis"  */
/*  in "Progress in Speech Synthesis" eds van Santen, J. et al. Springer */
/*  pp 157-172, 1996                                                     */
/*  Rivest, R.L. "Learning decision lists", Machine Learning 2:229-246   */
/*  1987                                                                 */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include <cstring>
#include "EST_Wagon.h"
#include "EST_FMatrix.h"
#include "EST_multistats.h"

static WDlist *wagon_decision_list();
static void do_dlist_summary(WDlist *dlist, WDataSet &dataset);
static WDlist *dlist_score_question(WQuestion &q, WVectorList &ds);

WNode *wgn_build_dlist(float &score,ostream *output)
{
    WDlist *dlist;

    dlist = wagon_decision_list();

    *output << *dlist;

    if (wgn_test_dataset.width() > 0)  // load in test data
	do_dlist_summary(dlist,wgn_test_dataset);
    else
	do_dlist_summary(dlist,wgn_dataset);

    score = 0;
    return 0;
}

static void do_dlist_summary(WDlist *dlist, WDataSet &dataset)
{
    // Test dlist against data to get summary of results
    EST_StrStr_KVL pairs;
    EST_StrList lex;
    EST_Litem *p;
    EST_String predict,real;
    int i,type;

    for (p=dataset.head(); p != 0; p=p->next())
    {
	predict = (EST_String)dlist->predict(*dataset(p));
	type = dataset.ftype(0);
	real = wgn_discretes[type].name(dataset(p)->get_int_val(0));
	pairs.add_item(real,predict,1);
    }
    for (i=0; i<wgn_discretes[dataset.ftype(0)].length(); i++)
	lex.append(wgn_discretes[dataset.ftype(0)].name(i));

    const EST_FMatrix &m = confusion(pairs,lex);
    print_confusion(m,pairs,lex);
    
}

static WDlist *wagon_decision_list()
{
    // For each possible question, measure it using 
    // yarowsky's loglikelihood ratio
    // Abs(log(P(class_1|question_j)/P(class_2|question_j)))
    // Output it for external sorting 
    // Hmm this implies binary classes 
    int i,cl;
    WQuestion ques;
    WDlist *dlist, *d;

    dlist=0;
    for (i=1;i < wgn_dataset.width(); i++)
    {
	if (wgn_dataset.ftype(i) == wndt_ignore)
	    continue;
	else if (wgn_dataset.ftype(i) >= wndt_class)
	{
	    ques.set_fp(i);
	    ques.set_oper(wnop_is);
	    for (cl=0; cl < wgn_discretes[wgn_dataset.ftype(i)].length(); cl++)
	    {
		ques.set_operand1(EST_Val(cl));
		d = dlist_score_question(ques,wgn_dataset);
		if (d != 0)
		    dlist = add_to_dlist(dlist,d);
	    }
	}
    }

    return dlist;
}

static WDlist *dlist_score_question(WQuestion &q, WVectorList &ds)
{
    // score this question for decision lists
    // the sum of the impurities when ds is split with this question
    WImpurity y;
    EST_Litem *d;
    WVector *wv;
    int i;

    for (i=0,d=ds.head(); d != 0; d=d->next(),i++)
    {
	wv = ds(d);
	if (q.ask(*wv) == TRUE)
	    y.cumulate((*wv)[0]);
    }

    if (y.samples() > wgn_min_cluster_size)
    {
	q.set_yes((int)y.samples());

	EST_DiscreteProbDistribution &pd = y.pd();
	
	// Generalizing the formula in Yarowsky (pp157-172) we modify it
	// to take absolute log-likelihood ration of the most probability
	// of the most probable over the rest
	EST_String t;
	double p;
	double f;
	WDlist *n = new WDlist;
	n->set_question(q);

	t = pd.most_probable(&p);
	f = pd.frequency(t);
	n->set_score(fabs(log((f+0.0001)/(pd.samples()-f+0.0001))));
	n->set_best(t,(int)f,(int)pd.samples());

#if 0 // original two-case code
        int freqa, freqb;
	pd.item_freq(0,s,freqa);
	pd.item_freq(1,s,freqb);

	n->set_score(fabs(log((0.0001+freqa)/(0.0001+freqb))));
	n->set_freqs(freqa,freqb);
#endif
	return n;
    }

    return 0;
}

EST_Val WDlist::predict(const WVector &d)
{
    if (p_question.ask(d))
	return p_token;
    else if (next == 0)
	return "guess";  // should be a priori most probable as dlist can't help
    else 
	return next->predict(d);
}

WDlist *add_to_dlist(WDlist *l, WDlist *a)
{
    // Add a to l at appropriate place in score order 
    WDlist *p,*lp;
    
    if (l == 0)
	return a;
    else
    {
	for (lp=0,p=l; p != 0; lp=p,p=p->next)
	{
	    if (a->score() > p->score())
	    {
		a->next = p;
		if (lp == 0)
		    return a;
		else
		{
		    lp->next = a;
		    return l;
		}
	    }
	}
	// add to end
	lp->next = a;
    }
    return l;
}

ostream &operator <<(ostream &s, WDlist &dlist)
{
    s << endl;
    s << "(";
    s << dlist.p_question;
    s << " ((";
    s << dlist.p_score;
    s << " " << dlist.p_freq << " " << dlist.p_samples << 
	" " << dlist.p_token << "))";
    if (dlist.next != 0)
	s << *dlist.next;
    else
	s << endl;
    s << ")";

    return s;
}
