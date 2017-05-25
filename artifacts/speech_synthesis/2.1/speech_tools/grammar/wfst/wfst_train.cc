/*************************************************************************/
/*                                                                       */
/*                  Language Technologies Institute                      */
/*                     Carnegie Mellon University                        */
/*                      Copyright (c) 1999-2003                          */
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
/*  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK         */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE      */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                     Author :  Alan W Black                            */
/*                     Date   :  October 1999                            */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Training method to split states of existing WFST based on data to     */
/* optimize entropy                                                      */
/*                                                                       */
/* Confusing as this has nothing to do with the modelling                */
/* technique known as "maximum entropy"                                  */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include <cstdlib>
#include "EST_WFST.h"
#include "wfst_aux.h"
#include "EST_Token.h"
#include "EST_simplestats.h"

VAL_REGISTER_TYPE_NODEL(trans,EST_WFST_Transition)
SIOD_REGISTER_CLASS(trans,EST_WFST_Transition)
VAL_REGISTER_CLASS(pdf,EST_DiscreteProbDistribution)
SIOD_REGISTER_CLASS(pdf,EST_DiscreteProbDistribution)

static LISP *find_state_usage(EST_WFST &wfst, LISP data);
static double entropy(const EST_WFST_State *s);
static LISP *find_state_entropies(const EST_WFST &wfst, LISP *data);
EST_WFST_Transition *find_best_trans_split(EST_WFST &wfst,
					   int split_state,
					   LISP *data);
static LISP find_best_split(EST_WFST &wfst,	
			    int split_state_name,
			    LISP *data);
static double find_score_if_split(EST_WFST &wfst,
				  int fromstate,
				  EST_WFST_Transition *trans,
				  LISP *data);
static LISP find_split_pdfs(EST_WFST &wfst,
			    int split_state_name,
			    LISP *data,
			    EST_DiscreteProbDistribution &pdf_all);
static double score_pdf_combine(EST_DiscreteProbDistribution &a,
				EST_DiscreteProbDistribution &b,
				EST_DiscreteProbDistribution &all);
#if 0
static void split_state(EST_WFST &wfst, EST_WFST_Transition *trans);
#endif
static void split_state(EST_WFST &wfst, LISP trans_list, int ostate);

LISP load_string_data(EST_WFST &wfst,EST_String &filename)
{
    // Load in sentences into data table, assume sentence per line
    EST_TokenStream ts;
    LISP ss = NIL;
    EST_String t;
    int id;
    int i,j;
    
    if (ts.open(filename) == -1)
	EST_error("wfst_train: failed to read data from \"%s\"",
			  (const char *)filename);

    i = 0;
    j = 0;
    while (!ts.eof())
    {
	LISP s = NIL;
	do
	{
	    t = (EST_String)ts.get();
	    id = wfst.in_symbol(t);
	    if (id == -1)
	    {
		cerr << "wfst_train: data contains unknown symbol \"" <<
		    t << "\"" << endl;
	    }
	    s = cons(flocons(id),s);
	    j++;
	}
	while (!ts.eoln() && !ts.eof());
	i++;
	ss = cons(reverse(s),ss);
    }

    printf("wfst_train: loaded %d lines of %d tokens\n",
	   i,j);

    return reverse(ss);
}

static LISP *find_state_usage(EST_WFST &wfst, LISP data)
{
    // Builds list of states, and which data points the represent
    LISP *state_data = new LISP[wfst.num_states()];
    static LISP ddd = NIL;
    int s,i,id;
    LISP d,w;
    EST_WFST_Transition *trans;
//    EST_Litem *tp;

    if (ddd == NIL)
	gc_protect(&ddd);

    ddd = NIL;

    wfst.start_cumulate();   // zero existing weights

    for (i=0; i < wfst.num_states(); i++)
    {
	state_data[i] = NIL;
	ddd = cons(state_data[i],ddd);
//	// smoothing
//	for (tp=wfst.state(i)->transitions.head(); tp != 0; tp = tp->next())
//	    wfst.state(i)->transitions(tp)->set_weight(1);
    }

    for (i=0,d=data; d; d=cdr(d),i++)
    {
	s = wfst.start_state();
	for (w=car(d); w; w=cdr(w))
	{
	    state_data[s] = cons(w,state_data[s]);
	    id = get_c_int(car(w));
	    trans = wfst.find_transition(s,id,id);
	    if (!trans)
	    {
		printf("sentence %d not in language, skipping\n",i);
		continue;
	    }
	    else
	    {
		trans->set_weight(trans->weight()+1);
		s = trans->state();
	    }
	}
    }
	
    wfst.stop_cumulate();
    return state_data;
}

static double entropy(const EST_WFST_State *s)
{
    double sentropy,w;
    EST_Litem *tp;
    for (sentropy=0,tp=s->transitions.head(); tp != 0; tp = tp->next())
    {
	w = s->transitions(tp)->weight();  /* the probability */
	if (w > 0)
	    sentropy += w * log(w);
    }
    return -1 * sentropy;
}

void wfst_train(EST_WFST &wfst, LISP data)
{
    LISP *state_data;
    LISP *state_entropies;
    LISP best_trans_list = NIL;
    int c=0,i, max_entropy_state;
    gc_protect(&data);

    while (1)
    {
	// Build table of state to points in data, and cumulate transitions
	state_data = find_state_usage(wfst,data);

	/* find entropy for each state (sorted) */
	state_entropies = find_state_entropies(wfst,state_data);

	max_entropy_state = -1;
	for (i=0; i < wfst.num_states(); i++)
	{
//	    double me = (double)get_c_float(car(state_entropies[i]));
	    max_entropy_state = get_c_int(cdr(state_entropies[i]));
//	    printf("trying %d %g\n",max_entropy_state,me);

//	    best_trans = find_best_trans_split(wfst,max_entropy_state,
//					       state_data);
	    best_trans_list = find_best_split(wfst,max_entropy_state,
					      state_data);
	    if (best_trans_list != NIL)
		break;
//	    else
//		printf("No best trans\n");
	}
	delete [] state_entropies;

	if (max_entropy_state == -1)
	{
	    printf("No new max_entropy state\n");
	    break;
	}
	if (best_trans_list == NIL)
	{
	    printf("No best_trans in max_entropy state\n");
	    break;
	}

        /* for each transition *entering* max_entropy_state */
        /*     find entropy if it were split          */
        /*     find best split                      */

        /* print stats */
        /* some sort of stop check */
	c++;
	printf("c is %d\n",c);
	if (c > 5000)
	{
	    printf("reached cycle end %d\n",c);
	    break;
	}
        /* split on best split                      */
        split_state(wfst, best_trans_list, max_entropy_state);
	
	if ((c % 100) == 0)
	{
	    EST_String chkpntname = "chkpnt";
	    char bbb[7];
	    sprintf(bbb,"%03d",c);
	    wfst.save(chkpntname+bbb+".wfst");
	}

	delete [] state_data;
	user_gc(NIL);
    }
}

static int me_compare_function(const void *a, const void *b)
{
    LISP la;
    LISP lb;
    la = *(LISP *)a;
    lb = *(LISP *)b;

    float fa = get_c_float(car(la));
    float fb = get_c_float(car(lb));

    if (fa < fb)
	return 1;
    else if (fa == fb)
	return 0;
    else
	return -1;
}

static LISP *find_state_entropies(const EST_WFST &wfst, LISP *data)
{
    double all_entropy = 0;
    int i;
    double sentropy;
    LISP *slist = new LISP[wfst.num_states()];
    static LISP ddd = NIL;

    if (ddd == NIL)
	gc_protect(&ddd);
    ddd = NIL;
    
    for (i=0; i < wfst.num_states(); i++)
    {
	const EST_WFST_State *s = wfst.state(i);
	sentropy = entropy(s);
//	printf("dlength is %d %d\n",i,siod_llength(data[i]));
	all_entropy += sentropy * siod_llength(data[i]);
	slist[i] = cons(flocons(sentropy),flocons(i));
	ddd = cons(slist[i],ddd);
    }
    printf("average entropy is %g\n",all_entropy/i);

    qsort(slist,wfst.num_states(),sizeof(LISP),me_compare_function);

    return slist;
}

static LISP find_best_split(EST_WFST &wfst,	
			    int split_state_name,
			    LISP *data)
{
    // Find the best partition of incoming translations that
    // minimises entropy
    EST_DiscreteProbDistribution pdf_all(&wfst.in_symbols());
    EST_DiscreteProbDistribution *a_pdf, *b_pdf;
    LISP splits,s,dd,r;
    LISP *ssplits;
    gc_protect(&splits);
    EST_String sname;
    int b,best_b;
    EST_Litem *i;
    int num_pdfs;
    double best_score, score, sfreq;

    for (dd = data[split_state_name]; dd; dd = cdr(dd))
	pdf_all.cumulate(get_c_int(car(car(dd))));
    splits = find_split_pdfs(wfst,split_state_name,data,pdf_all);
    if (siod_llength(splits) < 2)
	return NIL;
    ssplits = new LISP[siod_llength(splits)];
    for (num_pdfs=0,s=splits; s != NIL; s=cdr(s),num_pdfs++)
	ssplits[num_pdfs] = car(s);

    qsort(ssplits,num_pdfs,sizeof(LISP),me_compare_function);
    // Combine trans pdfs in pdfs until more combination doesn't improve
    while (1)
    {

	best_score = get_c_float(car(ssplits[0]));
	best_b = -1;
	a_pdf = pdf(car(cdr(cdr(ssplits[0]))));
        for (b=1; b < num_pdfs; b++)
	{
	    if (ssplits[b] == NIL)
		continue;
	    score = score_pdf_combine(*a_pdf,*pdf(car(cdr(cdr(ssplits[b])))),
				      pdf_all);
	    if (score < best_score)
	    {
		best_score = score;
		best_b = b;
	    }
	}

	// combine a and b
	if (best_b == -1)
	    break;
	else
	{
	    // combine a and b
	    // Add trans to 0
	    setcar(cdr(ssplits[0]),
		   append(car(cdr(ssplits[0])),
			  car(cdr(ssplits[best_b]))));
	    setcar(ssplits[0], flocons(best_score));
	    // Update 0's pdf with values from best_b's
	    b_pdf = pdf(car(cdr(cdr(ssplits[best_b]))));
	    for (i=b_pdf->item_start(); !b_pdf->item_end(i);
		 i = b_pdf->item_next(i))
	    {
		b_pdf->item_freq(i,sname,sfreq);
		a_pdf->cumulate(i,sfreq);
	    }
	    ssplits[best_b] = NIL;
	}

    }
    
    printf("score %g ",(double)get_c_float(car(ssplits[0])));
    for (dd=car(cdr(ssplits[0])); dd; dd=cdr(dd))
	printf("%s ",(const char *)wfst.in_symbol(trans(car(dd))->in_symbol()));
    printf("\n");
    gc_unprotect(&splits);
    r = car(cdr(ssplits[0]));
    delete [] ssplits;
    return r;
}

static double score_pdf_combine(EST_DiscreteProbDistribution &a,
				EST_DiscreteProbDistribution &b,
				EST_DiscreteProbDistribution &all)
{
    // Find score of (a+b) vs (all-(a+b))
    EST_DiscreteProbDistribution ab(a);
    EST_DiscreteProbDistribution all_but_ab(all);
    EST_Litem *i;
    EST_String sname;
    double sfreq, score;
    for (i=b.item_start(); !b.item_end(i);
	 i = b.item_next(i))
    {
	b.item_freq(i,sname,sfreq);
	ab.cumulate(i,sfreq);
    }
    
    for (i=ab.item_start(); !ab.item_end(i);
	 i = ab.item_next(i))
    {
	ab.item_freq(i,sname,sfreq);
	all_but_ab.cumulate(i,-1*sfreq);
    }
    
    score = (ab.entropy() * ab.samples()) +
	(all_but_ab.entropy() * all_but_ab.samples());

    return score;

}

static LISP find_split_pdfs(EST_WFST &wfst,
			    int split_state_name,
			    LISP *data,
			    EST_DiscreteProbDistribution &pdf_all)
{
    // Find following pdfs for each incoming transition as if they where
    // split to a new state
    int i,id, in;
    EST_Litem *tp;
    LISP pdfs = NIL,dd,ttt,p,t;
    EST_DiscreteProbDistribution empty;
    double value;

    for (i=0; i < wfst.num_states(); i++)
    {
	const EST_WFST_State *s = wfst.state(i);
	for (tp=s->transitions.head(); tp != 0; tp = tp->next())
	{
	    if ((s->transitions(tp)->state() == split_state_name)
		&& (s->transitions(tp)->weight() > 0))
	    {
		in = s->transitions(tp)->in_symbol();
		EST_DiscreteProbDistribution *pdf = 
		    new EST_DiscreteProbDistribution(&wfst.in_symbols());
		for (dd = data[i]; dd; dd = cdr(dd))
		{
		    id = get_c_int(car(car(dd)));
		    if (id == in)
		    {   // This one would go to the new state so we count it
			if (cdr(car(dd))) // not end of data string
			    pdf->cumulate(get_c_int(car(cdr(car(dd)))));
		    }
		}
		// value, list of trans, pdf
		value = score_pdf_combine(*pdf,empty,pdf_all);
		if ((value > 0) && // ignore transitions with no data
		    (pdf->samples() > 10))// and those with only a few data pnts
		{
		    t = siod(s->transitions(tp));
		    p = siod(pdf);
		    ttt = cons(flocons(value),
			       cons(cons(t,NIL),
				    cons(p,NIL)));
		    pdfs = cons(ttt,pdfs);
		}
		else
		    delete pdf;
	    }
	}
    }
    return pdfs;
}

EST_WFST_Transition *find_best_trans_split(EST_WFST &wfst,
					   int split_state_name,
					   LISP *data)
{
    EST_Litem *tp;
    EST_WFST_Transition *best_trans = 0;
    const EST_WFST_State *split_state = wfst.state(split_state_name);
    double best_score,bb;
    int i;

    best_score = entropy(split_state)*siod_llength(data[split_state_name]);
//    printf("unsplit score %g\n",best_score);

    /* For each transition going to split_state */
    for (i=1; i < wfst.num_states(); i++)
    {
	const EST_WFST_State *s = wfst.state(i);
	for (tp=s->transitions.head(); tp != 0; tp = tp->next())
	{
	    if ((wfst.state(s->transitions(tp)->state()) == split_state) &&
		(s->transitions(tp)->weight() > 0))
	    {
		bb = find_score_if_split(wfst,i,s->transitions(tp),data);
//		cout << i << " " 
//		     << wfst.in_symbol(s->transitions(tp)->in_symbol()) << " "
//		     << s->transitions(tp)->state() << " " << bb << endl;
		if (bb == -1)  /* didn't find a split */
		    continue;
		if (bb < best_score)
		{
		    best_score = bb;
		    best_trans = s->transitions(tp);
		}
	    }
	}
    }

    if (best_trans)
	cout << "best " << wfst.in_symbol(best_trans->in_symbol()) << " "
	     << best_trans->weight() << " " 
 	     << best_trans->state() << " " << best_score << endl;
    return best_trans;
}

static double find_score_if_split(EST_WFST &wfst,
                                  int fromstate,
				  EST_WFST_Transition *trans,
				  LISP *data)
{
    double ent_split;
    double ent_remain;
    double score;
    EST_DiscreteProbDistribution pdf_split(&wfst.in_symbols());
    EST_DiscreteProbDistribution pdf_remain(&wfst.in_symbols());
    int in, tostate, id;
    EST_Litem *i;
    double sfreq;
    EST_String sname;

    ent_split = ent_remain = 32*32*32*32;
    LISP dd;

//    printf("considering %d %s %g %d\n",
//	   fromstate,
//	   (const char *)wfst.in_symbol(trans->in_symbol()),
//	   trans->weight(),
//	   trans->state());

    /* find entropy of possible new state */
    /* for each data point through fromstate */
    in = trans->in_symbol();
    for (dd = data[fromstate]; dd; dd = cdr(dd))
    {
	id = get_c_int(car(car(dd)));
	if (id == in)
	{   // This one would go to the new state so we count it
	    if (cdr(car(dd))) // not end of data string
		pdf_split.cumulate(get_c_int(car(cdr(car(dd)))));
	}
    }
    if (pdf_split.samples() > 0)
	ent_split = pdf_split.entropy();
    /* find entropy of old state minus trans into it */
    tostate = trans->state();
    // Actually only need to do this once per state
    for (dd = data[tostate]; dd; dd = cdr(dd))
	pdf_remain.cumulate(get_c_int(car(car(dd))));
    // Subtract the bit thats split
    for (i=pdf_split.item_start(); !pdf_split.item_end(i);
	 i = pdf_split.item_next(i))
    {
	pdf_split.item_freq(i,sname,sfreq);
	pdf_remain.cumulate(i,-1*sfreq);
    }
    if (pdf_remain.samples() > 0)
	ent_remain = pdf_remain.entropy();

    if ((pdf_remain.samples() == 0) ||
	(pdf_split.samples() == 0))
	return -1;

    score = (ent_remain * pdf_remain.samples()) +
	(ent_split * pdf_split.samples());
//    printf("tostate %d remain %g %d split %g %d score %g\n",
//	   tostate, ent_remain, (int)pdf_remain.samples(),
//	   ent_split, (int)pdf_split.samples(), score);

    return score;
}

#if 0
static void split_state(EST_WFST &wfst, EST_WFST_Transition *trans)
{
    /* Split off a new state for given trans.  Add transitions    */
    /* to this new state for all transitions in (old) state trans */
    /* goes to                                                    */
    EST_Litem *tp;
    int nstate = wfst.add_state(wfst_final);
    int ostate = trans->state();

//    printf("state %d entropy %g\n",ostate,entropy(wfst.state(ostate)));
    /* must be done before adding the new transitions to nstate */
    trans->set_state(nstate);

    for (tp=wfst.state(ostate)->transitions.head(); tp != 0; tp = tp->next())
    {
	wfst.state_non_const(nstate)->
	    add_transition(0.0,  /* weight will be filled in later*/
			   wfst.state(ostate)->transitions(tp)->state(),
			   wfst.state(ostate)->transitions(tp)->in_symbol(),
			   wfst.state(ostate)->transitions(tp)->out_symbol());

    }
//    printf(" nstate %d entropy %g\n",nstate,entropy(wfst.state(nstate)));
//    printf(" ostate %d entropy %g\n",ostate,entropy(wfst.state(ostate)));

}
#endif

static void split_state(EST_WFST &wfst, LISP trans_list, int ostate)
{
    /* Split off a new state for given trans.  Add transitions    */
    /* to this new state for all transitions in (old) state trans */
    /* goes to                                                    */
    EST_Litem *tp;
    int nstate = wfst.add_state(wfst_final);
    LISP t;

    /* must be done before adding the new transitions to nstate */
    for (t=trans_list; t; t=cdr(t))
	trans(car(t))->set_state(nstate);

    for (tp=wfst.state(ostate)->transitions.head(); tp != 0; tp = tp->next())
    {
	wfst.state_non_const(nstate)->
	    add_transition(0.0,  /* weight will be filled in later*/
			   wfst.state(ostate)->transitions(tp)->state(),
			   wfst.state(ostate)->transitions(tp)->in_symbol(),
			   wfst.state(ostate)->transitions(tp)->out_symbol());

    }
}

