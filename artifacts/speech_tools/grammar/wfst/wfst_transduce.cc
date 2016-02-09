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
/*                     Author :  Alan W Black                            */
/*                     Date   :  December 1997                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Transduction using a WFST                                             */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include "EST_WFST.h"

/** An internal class in transduction of WFSTs holding intermediate
    state information.
*/
class wfst_tstate {
  public:
    int state;
    EST_IList outs;
    float score;
};

ostream &operator << (ostream &s, const wfst_tstate &state)
{
  (void)state;
  return s << "<<wfst_tstate>>";
}

Declare_TList(wfst_tstate)

#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_TList.cc"

Instantiate_TList(wfst_tstate)

#endif

typedef EST_TList<wfst_tstate> wfst_tstate_list;

static void add_transduce_mstate(const EST_WFST &wfst,
				 const wfst_tstate &cs,
				 wfst_translist &tranlist,
				 wfst_tstate_list &ns);

int transduce(const EST_WFST &wfst,const EST_StrList &in,EST_StrList &out)
{
    // Map names to internal ints before transduction
    EST_Litem *p;
    EST_IList in_i,out_i;
    int r;

    for (p=in.head(); p != 0; p=p->next())
	in_i.append(wfst.in_symbol(in(p)));

    r = transduce(wfst,in_i,out_i);

    for (p=out_i.head(); p != 0; p=p->next())
	out.append(wfst.out_symbol(out_i(p)));

    return r;
}

int transduce(const EST_WFST &wfst,const EST_IList &in,EST_IList &out)
{
    // Transduce input stream to an output stream
    EST_Litem *i,*cs;
    int r=FALSE;
    wfst_tstate_list *current_ms = new wfst_tstate_list;
    wfst_tstate start_state;
    wfst_translist ss_eps_trans;

    start_state.state = wfst.start_state();
    start_state.score = 0;
    current_ms->append(start_state);
    // Add any epsilon accessible states
    wfst.transduce(wfst.start_state(),wfst.in_epsilon(),ss_eps_trans);
    add_transduce_mstate(wfst,start_state,ss_eps_trans,*current_ms);

    for (i=in.head(); i != 0; i=i->next())
    {
	wfst_tstate_list *ns = new wfst_tstate_list;

	for (cs=current_ms->head(); cs != 0; cs=cs->next())
	{   // For each state in current update list of new states
	    wfst_translist translist;
	    wfst.transduce((*current_ms)(cs).state,in(i),translist);
	    add_transduce_mstate(wfst,(*current_ms)(cs),translist,*ns);
	}
	// Using pointers to avoid having to copy the state list
	delete current_ms; 
	current_ms = ns;  

	if (current_ms->length() == 0)
	    break;  // give up, no transition possible
    }
    // current_ms will contain the list of possible transitions
    if (current_ms->length() > 1)
	cerr << "WFST: found " << current_ms->length() << " transductions" <<
	    endl;
    // should find "best" but we'll find longest at present
    // Choose the longest (should be based on score)
    for (cs = current_ms->head(); cs != 0; cs=cs->next())
    {
	if ((wfst.final((*current_ms)(cs).state)) &&
	    ((*current_ms)(cs).outs.length() > out.length()))
	{
	    r = TRUE;
	    out = (*current_ms)(cs).outs;
	}
    }
    delete current_ms;
    return r;
}

static void add_transduce_mstate(const EST_WFST &wfst,
				 const wfst_tstate &cs,
				 wfst_translist &translist,
				 wfst_tstate_list &ns)
{
    // For each possible transduction of in from cs.state in WFST
    // add it to ns.
    // This should really only add states if they are not already there
    // but you really need stae plus recognized path to get all 
    // transductions so a tree structure would be better.
    EST_Litem *t;

    // Add new states to ns if not already there
    for (t=translist.head(); t != 0; t=t->next())
    {
	// Declare a new one and put it on the end of the list
	// before we fill its values, this saves a copy
	wfst_tstate tts;
	ns.append(tts); 
	wfst_tstate &ts = ns.last();
	
	ts.state = translist(t)->state();
	// the combination is probably WFST dependant
	ts.score = translist(t)->weight()+cs.score;
	// copying the outs up to now (pity)
	ts.outs = cs.outs;
	ts.outs.append(translist(t)->out_symbol());
	
	// Also any potential epsilon transitions for this new state
	wfst_translist etranslist;
	wfst.transduce(ts.state,wfst.in_epsilon(),etranslist);
	add_transduce_mstate(wfst,ts,etranslist,ns);
    }
}

int recognize(const EST_WFST &wfst,const EST_StrList &in,int quiet)
{
    // Map names to internal ints before recognition
    EST_Litem *p;
    EST_IList in_i,out_i;
    int i,o;
    int r;
    
    for (p=in.head(); p != 0; p=p->next())
    {
	if (in(p).contains("/"))
	{
	    i = wfst.in_symbol(in(p).before("/"));
	    o = wfst.out_symbol(in(p).after("/"));
	}
	else
	{
	    i = wfst.in_symbol(in(p));
	    o = wfst.out_symbol(in(p));
	}
	in_i.append(i);
	out_i.append(o);
    }
    
    r = recognize(wfst,in_i,out_i,quiet);
    
    return r;
}

int recognize(const EST_WFST &wfst,const EST_IList &in, 
	      const EST_IList &out, int quiet)
{
    int state = wfst.start_state();
    EST_Litem *p,*q;
    int nstate;
    
    for (p=in.head(),q=out.head();
	 ((p != 0) && (q != 0));
	 p=p->next(),q=q->next())
    {
	nstate = wfst.transition(state,in(p),out(q));
	if (!quiet)
	    printf("state %d %s/%s -> %d\n",state,
		   (const char *)wfst.in_symbol(in(p)),
		   (const char *)wfst.out_symbol(out(q)),
		   nstate);
	state = nstate;
	if (state == WFST_ERROR_STATE)
	    return FALSE;
    }
    
    if (p != q)
    {  
	cerr << "wfst recognize: in/out tapes of different lengths"
	    << endl;
	return FALSE;
    }
    
    if (wfst.final(state))
	return TRUE;
    else 
	return FALSE;
}

int recognize_for_perplexity(const EST_WFST &wfst,
			     const EST_StrList &in,
			     int quiet,
			     float &count,
			     float &sumlogp)
{
    // Map names to internal ints before recognition
    EST_Litem *p;
    EST_IList in_i,out_i;
    int i,o;
    int r;
    
    for (p=in.head(); p != 0; p=p->next())
    {
	if (in(p).contains("/"))
	{
	    i = wfst.in_symbol(in(p).before("/"));
	    o = wfst.out_symbol(in(p).after("/"));
	}
	else
	{
	    i = wfst.in_symbol(in(p));
	    o = wfst.out_symbol(in(p));
	}
	in_i.append(i);
	out_i.append(o);
    }
    
    r = recognize_for_perplexity(wfst,in_i,out_i,quiet,count,sumlogp);
    
    return r;
}

int recognize_for_perplexity(const EST_WFST &wfst,
			     const EST_IList &in, 
			     const EST_IList &out, 
			     int quiet,
			     float &count,
			     float &sumlogp)
{
    int state = wfst.start_state();
    EST_Litem *p,*q;
    int nstate;
    float prob;
    count = 0;
    sumlogp = 0;
    
    for (p=in.head(),q=out.head();
	 ((p != 0) && (q != 0));
	 p=p->next(),q=q->next())
    {
	nstate = wfst.transition(state,in(p),out(q),prob);
	count++;
	if (prob > 0)
	    sumlogp += log(prob);
	else
	    sumlogp += -100;  // bad hack
	if (!quiet)
	    printf("state %d %s/%s -> %d\n",state,
		   (const char *)wfst.in_symbol(in(p)),
		   (const char *)wfst.out_symbol(out(q)),
		   nstate);
	state = nstate;
	if (state == WFST_ERROR_STATE)
	    return FALSE;
    }
    
    if (p != q)
    {  
	cerr << "wfst recognize: in/out tapes of different lengths"
	    << endl;
	return FALSE;
    }
    
    if (wfst.final(state))
	return TRUE;
    else 
	return FALSE;
}

