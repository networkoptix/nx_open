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
/*                     Date   :  November 1997                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Basic WFST operations: minimization, determinization, intersection,   */
/*   union, composition                                                  */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include <cstdlib>
#include "EST_WFST.h"
#include "wfst_aux.h"
#include "EST_String.h"
#include "EST_TList.h"
#include "EST_TKVL.h"
#include "EST_THash.h"

Declare_TList_T(EST_WFST_MultiState *,EST_WFST_MultiStateP)

  // Declare_KVL(int, EST_IList)

	typedef EST_TKVI<int, EST_IList> KVI_int_EST_IList_t; 
	typedef EST_TKVL<int, EST_IList> KVL_int_EST_IList_t; 
	
	static EST_IList int_EST_IList_kv_def_EST_IList_s; 
	static int int_EST_IList_kv_def_int_s; 
	
	template <> EST_IList *EST_TKVL< int, EST_IList >::default_val=&int_EST_IList_kv_def_EST_IList_s; 
	template <> int *EST_TKVL< int, EST_IList >::default_key=&int_EST_IList_kv_def_int_s; 
	
	Declare_TList_N(KVI_int_EST_IList_t, 0)


#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_TList.cc"

  Instantiate_TList_T_MIN(EST_WFST_MultiState *,EST_WFST_MultiStateP)

#include "../base_class/EST_TKVL.cc"

	  //  Instantiate_KVL(int, EST_IList)

        template class EST_TKVL<int, EST_IList>; 
        template class EST_TKVI<int, EST_IList>; 
// ostream &operator<<(ostream &s, EST_TKVI< int , EST_IList > const &i){  return s << i.k << "\t" << i.v << "\n"; } 
//	ostream& operator << (ostream& s,  EST_TKVL< int , EST_IList > const &l) {EST_Litem *p; for (p = l.list.head(); p ; p = p->next()) s << l.list(p).k << "\t" << l.list(p).v << endl; return s;} 
        Instantiate_TIterator_T(KVL_int_EST_IList_t, KVL_int_EST_IList_t::IPointer_k, int, KVL_int_EST_IList_kitt) 
        Instantiate_TStructIterator_T(KVL_int_EST_IList_t, KVL_int_EST_IList_t::IPointer, KVI_int_EST_IList_t, KVL_int_EST_IList_itt) 
        Instantiate_TIterator_T(KVL_int_EST_IList_t, KVL_int_EST_IList_t::IPointer, KVI_int_EST_IList_t, KVL_int_EST_IList_itt) 

	  // Instantiate_TList(KVI_int_EST_IList_t)

	template class EST_TList< TLIST_KVI_int_EST_IList_t_VAL >; 
	template class EST_TItem< TLIST_KVI_int_EST_IList_t_VAL >; 
	template const char *error_name(EST_TList< KVI_int_EST_IList_t > val); 

        Instantiate_TIterator_T( EST_TList<KVI_int_EST_IList_t>, EST_TList<KVI_int_EST_IList_t>::IPointer, KVI_int_EST_IList_t, TList_KVI_int_EST_IList_t_itt);


#endif

typedef EST_TList<EST_WFST_MultiState *> Agenda;

static enum wfst_state_type intersect_state_type(wfst_list &wl,
						 EST_WFST_MultiState *ms);
static int check_distinguished(const EST_WFST &nmwfst,
			       int p, int q,
			       wfst_marks &marks,
			       wfst_assumes &assumptions);

void EST_WFST_MultiState::add(int i)
{
    // If of set type only add it if its not already there
    EST_Litem *p;

    if (p_type == wfst_ms_set)
	for (p=head(); p != 0; p=p->next())
	{
	    if ((*this)(p) == i)
		return;
	    else if (i < (*this)(p))  // keep the list ordered 
	    {
		insert_before(p,i);
		return;
	    }
	}

    append(i);
}

int multistate_index(EST_WFST_MultiStateIndex &index,
		     EST_WFST_MultiState *ms,int proposed) 
{
    // Returns proposed if ms is not already in index, otherwise
    // returns the value that was proposed when it was first new.

    // I'll have to make this more efficient in future.
    EST_String istring("");
    EST_Litem *p;
    int ns,found;

    for (p=ms->head(); p != 0; p = p->next())
	istring += itoString((*ms)(p)) + " ";

    ns = index.val(istring,found);
    if (found)
	return ns;
    else
    {
        index.add_item(istring,proposed);
	return proposed;
    }
}

static int pair_check(EST_THash<int,int> &pairs_done, int i, int o, int odim)
{
    int p;
    int found;

    p = (i*odim)+o;  // unique number representing i/o pair

    pairs_done.val(p,found);
    if (!found)
    {   // first time seeing this pair
	pairs_done.add_item(p,1);
	return 0;
    }
    return 1;

}

void EST_WFST::determinize(const EST_WFST &ndwfst)
{
    // Determinise a non-deterministic WFST
    EST_WFST_MultiState *start_state,*nms,*current;
    int ns;
    Agenda multistate_agenda;
    EST_WFST_MultiStateIndex index(100);
    int i,o, new_name;
    int c=0;
    EST_Litem *sp, *tp;

    clear();
    p_in_symbols.copy(ndwfst.p_in_symbols);
    p_out_symbols.copy(ndwfst.p_out_symbols);

    // Create a starting state and add it to this WFST
    start_state = new EST_WFST_MultiState(wfst_ms_set);
    start_state->add(ndwfst.start_state());
    ndwfst.add_epsilon_reachable(start_state);
    
    p_start_state = add_state(ndwfst.ms_type(start_state));
    start_state->set_name(p_start_state);

    multistate_agenda.append(start_state);  // initialize agenda

    while (multistate_agenda.length() > 0)
    {
	EST_THash<int,int> pairs_done(100);
	current = multistate_agenda.first();
	multistate_agenda.remove(multistate_agenda.head());
	if ((c % 100) == 99)
	    cout << "Determinizing " << summary() << " Agenda " 
		<< multistate_agenda.length() << endl;
	c++;

	for (sp=current->head(); sp != 0; sp=sp->next())
	{
	    const EST_WFST_State *s = ndwfst.state((*current)(sp));
	    for (tp=s->transitions.head(); tp != 0; tp = tp->next())
	    {
		i = s->transitions(tp)->in_symbol();
		o = s->transitions(tp)->out_symbol();
		// Need to check if i/o has already been proposed
		if (pair_check(pairs_done,i,o,p_out_symbols.length()) == 1)
		    continue;  // already prosed those
//	for (i=0; i < p_in_symbols.length(); i++)
//	{   // start at 2 to skip any and epsilon characters -- hmm bad
//	    for (o=0; o < p_out_symbols.length(); o++)
//	    {
		if ((i==o) && (i==0)) 
		    continue;  // don't deal here with epsilon transitions
		nms = apply_multistate(ndwfst,current,i,o);
		if ((nms->length() == 0) ||
		    (ndwfst.ms_type(nms) == wfst_error))
		{
		    delete nms;
		    continue;    // no state to go to
		}
		new_name = multistate_index(index,nms,p_num_states);
		if (new_name == p_num_states)  // genuinely new
		{   // create a real state and add it to the agenda
		    ns = add_state(ndwfst.ms_type(nms));
		    nms->set_name(ns);
		    multistate_agenda.append(nms);  
		}
		else
		{
		    nms->set_name(new_name);
		    delete nms;
		}

		// Add new transition to current state
		p_states[current->name()]
		    ->add_transition(nms->weight(),
				     nms->name(),
				     i,o);
		
	    }
	}
	delete current;

	// Probably want some progress summary
    }

}

EST_WFST_MultiState *EST_WFST::apply_multistate(const EST_WFST &wfst,
						EST_WFST_MultiState *ms,
						int in, int out) const
{
    // Apply in and out to ms and find all new states it becomes
    EST_Litem *p;
    EST_WFST_MultiState *new_ms = new EST_WFST_MultiState(wfst_ms_set);
    
    new_ms->clear();
    
    for (p=ms->head(); p != 0; p=p->next())
	// Add all new possible states from ms(p) given in/out
	wfst.transition_all((*ms)(p),in,out,new_ms);

    // Add epsilon reachable states from any states in multistates
    wfst.add_epsilon_reachable(new_ms);

    return new_ms;

}

enum wfst_state_type EST_WFST::ms_type(EST_WFST_MultiState *ms) const
{
    // Returns wfst_error if ms contains an error state, wfst_final
    // if there is at least one final and wfst_non_final
    EST_Litem *p;
    enum wfst_state_type r = wfst_nonfinal;

    for (p=ms->head(); p != 0; p = p->next())
	if (p_states((*ms)(p))->type() == wfst_error)
	    return wfst_error;
	else if (p_states((*ms)(p))->type() == wfst_licence)
	    // wfst_licence states are generated in KK compilation
	    r = wfst_licence;
	else if ((p_states((*ms)(p))->type() == wfst_final) &&
		 (r != wfst_licence))
	    r = wfst_final;

    if (r == wfst_licence)
	return wfst_nonfinal;
    else 
	return r;
}

void EST_WFST::transition_all(int state,
			      int in,
			      int out,
			      EST_WFST_MultiState *ms) const
{
    // Find all possible new states from state given in/out
    const EST_WFST_State *s = p_states(state);
    EST_Litem *i;

    for (i=s->transitions.head(); i != 0; i=i->next())
    {
	if ((in == s->transitions(i)->in_symbol()) &&
	    (out == s->transitions(i)->out_symbol()))
	    ms->add(s->transitions(i)->state());
    }
}

static int is_a_member(const EST_IList &ii, int i)
{
    for (EST_Litem *p=ii.head(); p != 0; p=p->next())
	if (ii(p) == i)
	    return TRUE;
    return FALSE;
}

void EST_WFST::add_epsilon_reachable(EST_WFST_MultiState *ms) const
{
    // As ms->add() adds in order we need to copy to a new list and append
    // to it any new epsilon accessible states
    EST_Litem *p;
    EST_IList ii;
    int ie = p_in_symbols.name(get_c_string(epsilon_label()));
    int oe = p_out_symbols.name(get_c_string(epsilon_label()));

    for (p=ms->head(); p != 0; p=p->next())
	ii.append((*ms)(p));

    for (p=ii.head(); p != 0; p=p->next())
    {
	const EST_WFST_State *s = p_states(ii(p));
	EST_Litem *i;

	for (i=s->transitions.head(); i != 0; i=i->next())
	{
	    if ((ie == s->transitions(i)->in_symbol()) &&
		(oe == s->transitions(i)->out_symbol()))
	    {
		// Add to end of ii if not already there
		int nstate = s->transitions(i)->state();
		if (!is_a_member(ii,nstate))
		{
		    ii.append(nstate);
		    ms->add(nstate); // gets added in order
		}
	    }
	}
    }
}

/************************************************************************/
/*  Intersection of a list of transducers, i.e. what is accepted by all */
/************************************************************************/
void EST_WFST::intersection(wfst_list &wl)
{
    // This is very similar to determinisation, similar complexity too
    EST_WFST_MultiState *start_state = new EST_WFST_MultiState(wfst_ms_list);
    EST_WFST_MultiState *nms,*current;
    int ns;
    Agenda multistate_agenda;
    EST_WFST_MultiStateIndex index(100);
    int i,o, new_name, n;
    EST_Litem *p,*q;
    int c=0;

    // Initialize this WFST from the given ones
    clear();
    p_in_symbols.copy(wl.first().p_in_symbols);
    p_out_symbols.copy(wl.first().p_out_symbols);

    // Determinize each input WFST and make a new start state consisting of 
    // the start states of each of the input WFSTs
    for (p=wl.tail(); p != 0; p=p->prev())
    {
	if (!wl(p).deterministic())
	{
	    cout << "...intersection deterministing" << endl;
	    EST_WFST tt = wl(p);
	    wl(p).determinize(tt);
	}
	start_state->add(wl(p).start_state());
    }
    
    p_start_state = add_state(intersect_state_type(wl,start_state));
    // Label multistate start start with single-state name
    start_state->set_name(p_start_state);

    multistate_agenda.append(start_state);  // initialize agenda

    while (multistate_agenda.length() > 0)
    {
	current = multistate_agenda.first();
	multistate_agenda.remove(multistate_agenda.head());
	if ((c % 100) == 99)
	    cout << "Intersection " << summary() << " Agenda " 
		<< multistate_agenda.length() << endl;
	c++;

	// For all possible in/out pairs
	for (i=0; i < p_in_symbols.length(); i++)
	{   // start at 2 to skip any and epsilon characters -- hmm bad
	    for (o=0; o < p_out_symbols.length(); o++)
	    {
		if ((i==o) && (i==0)) // shouldn't be epsilon/epsilon here 
		    continue;
		nms = new EST_WFST_MultiState(wfst_ms_list);
		// Increment multistate to new multistate for each individual
		// state using each WFST
		for (n=0,p=wl.head(),q=current->head(); 
		     (p != 0) && (q != 0);
		     p=p->next(),q=q->next(),n++)
		    nms->add(wl(p).transition((*current)(q),i,o));
		if (intersect_state_type(wl,nms) == wfst_error)
		{
		    delete nms;
		    continue;    // no state to go to
		}
		new_name = multistate_index(index,nms,p_num_states);
		if (new_name == p_num_states)  // genuinely new and unseen
		{   // create a real state and add it to the agenda
		    ns = add_state(intersect_state_type(wl,nms));
		    nms->set_name(ns);
		    multistate_agenda.append(nms);  
		}
		else  // already seen this state, and is already named
		{
		    nms->set_name(new_name);
		    delete nms;
		}

		// Add new transition to current state
		p_states[current->name()]
		    ->add_transition(nms->weight(),nms->name(),i,o);
	    }
	}
	delete current;
	// Probably want some progress summary
    }

}

static enum wfst_state_type intersect_state_type(wfst_list &wl,
						 EST_WFST_MultiState *ms)
{
    // Find the state type of the combined states
    // If any are error, return error, if one is nonfinal return nonfinal
    // otherwise its final
    EST_Litem *p,*q;
    enum wfst_state_type r = wfst_final;

    for (p=wl.head(),q=ms->head(); 
	 (p != 0) && (q != 0); 
	 p=p->next(),q=q->next())
    {
	if ((*ms)(q) == WFST_ERROR_STATE)
	    return wfst_error;

	enum wfst_state_type dd = wl(p).state((*ms)(q))->type();

	if (dd == wfst_error)
	    return wfst_error;
	else if (dd == wfst_nonfinal)
	    r = wfst_nonfinal;
    }
    return r;
}

void EST_WFST::intersection(const EST_WFST &a, const EST_WFST &b)
{
    // Intersect two WFSTs
    wfst_list wl;

    wl.append(a);
    wl.append(b);
    
    intersection(wl);
}

/*******************************************************************/
/*  Minimization is of complexity of O(n^2)                        */
/*******************************************************************/
void EST_WFST::minimize(const EST_WFST &nmwfst)
{
    // Minimize a WFST
    int p,q;
    wfst_marks marks(nmwfst.num_states());
    wfst_assumes assumptions;

    //  For each combination of states
    for (p=0; p < nmwfst.num_states()-1; p++)
	for (q=p+1; q < nmwfst.num_states(); q++)
	    check_distinguished(nmwfst,p,q,marks,assumptions);

    // The marked array now has all different and lists to equivalent states
    // Build an array mapping old name to new name.
    int num_new_states;
    int i;
    EST_IVector state_map;

    marks.find_state_map(state_map,num_new_states);

    // Build the new minimized WFST mapping existing transitions
    clear();
    p_in_symbols.copy(nmwfst.p_in_symbols);
    p_out_symbols.copy(nmwfst.p_out_symbols);

    init(num_new_states);
    p_start_state = state_map(nmwfst.p_start_state);

    for (i=0; i < nmwfst.num_states(); i++)
    {
	if (p_states[state_map(i)] == 0)
	    p_states[state_map(i)] = 
		copy_and_map_states(state_map,nmwfst.state(i),nmwfst);
    }

}

static int check_distinguished(const EST_WFST &nmwfst,
			       int p, int q,
			       wfst_marks &marks,
			       wfst_assumes &assumptions)
{
    // Check to see if these two are equivalent
    EST_Litem *t;
    EST_IList from_p,from_q;
    
    if (marks.distinguished(p,q))   // been here, done that
	return TRUE;
    else if (marks.undistinguished(p,q)) // been here too
	return FALSE;
    // Not been here yet so do some work to try to find out if
    // these states can be distinguished
    else if ((nmwfst.state(p)->type() != nmwfst.state(q)->type()) ||
	     (nmwfst.state(p)->num_transitions() != 
	      nmwfst.state(q)->num_transitions()))
    {	// Different final/non-final type or different number
	// of transitions so obviously different states
	marks.distinguish(p,q);
	return TRUE;
    }
    else 
    {   // Have to check their transitions individually
	for (t=nmwfst.state(p)->transitions.head(); t != 0; t=t->next())
	{
	    int in = nmwfst.state(p)->transitions(t)->in_symbol();
	    int out = nmwfst.state(p)->transitions(t)->out_symbol();
	    int y = nmwfst.state(p)->transitions(t)->state();
	    int z = nmwfst.transition(q,in,out);
	    if ((z == WFST_ERROR_STATE) || 
		(marks.distinguished(y,z)))
	    {   // no equiv transition or obviously different states
		marks.distinguish(p,q);
		return TRUE;
	    }
	    else if (equivalent_to(y,z,assumptions))
		continue;
	    else   // Potential equivalence, record y,z for later
	    {
		from_p.append(y);
		from_q.append(z);
	    }
	}
	// All transitions had potential match so only now
	// actually check their follow sets
	EST_Litem *yp, *zp;
	int tl = FALSE;
	if (assumptions.length() == 0)
	    tl = TRUE;
	// assume they are undistinguished
	add_assumption(p,q,assumptions);
	for (yp=from_p.head(),zp=from_q.head(); 
	     yp != 0; yp=yp->next(),zp=zp->next())
	    if (check_distinguished(nmwfst,from_p(yp),from_q(zp),
				    marks,
				    assumptions))
	    {
		marks.distinguish(p,q);  // set the distinguished
		assumptions.clear();
		return TRUE;
	    }
	// ok I give up, they are the same
	if (tl)
	{   // This is equivalent given the assumptions (and no
	    // higher level assumptions) so we can mark these states
	    // as undistinguished (and all the assumptions)
	    mark_undistinguished(marks,assumptions);
	    assumptions.clear();
	}
	return FALSE;
    }
}

void EST_WFST::extend_alphabets(const EST_WFST &b)
{
    // Extend current in/out alphabets to accommodate anything in b's
    // that are not in a's
    // This guarantees that the number in this will still be valid
    EST_StrList ivocab, ovocab;
    int i;
    
    for (i=0; i<p_in_symbols.length(); i++)
 	ivocab.append(in_symbol(i));
    for (i=0; i<b.p_in_symbols.length(); i++)
	if (!strlist_member(ivocab,b.in_symbol(i)))
	    ivocab.append(b.in_symbol(i));
    for (i=0; i<p_out_symbols.length(); i++)
	ovocab.append(out_symbol(i));
    for (i=0; i<b.p_out_symbols.length(); i++)
	if (!strlist_member(ovocab,b.out_symbol(i)))
	    ovocab.append(b.out_symbol(i));

    p_in_symbols.init(ivocab);
    p_out_symbols.init(ovocab);
}

EST_WFST_State *EST_WFST::copy_and_map_states(const EST_IVector &state_map,
					      const EST_WFST_State *s,
					      const EST_WFST &b) const
{
    // Copy s into new state mapping new states to those in state map
    EST_WFST_State *ns = new EST_WFST_State(state_map(s->name()));
    EST_Litem *i;

    ns->set_type(s->type());

    for (i=s->transitions.head(); i != 0; i=i->next())
    {
	int mapped_state= state_map(s->transitions(i)->state());
	if (mapped_state != WFST_ERROR_STATE)
	    ns->add_transition(s->transitions(i)->weight(),
		    mapped_state,
		    in_symbol(b.in_symbol(s->transitions(i)->in_symbol())),
		    out_symbol(b.out_symbol(s->transitions(i)->out_symbol())));
    }

    return ns;
}

/***********************************************************************/
/* Build a WFST which doesn't accept any of the strings that a accepts */
/* This keeps the same in/out alphabet                                 */
/***********************************************************************/
void EST_WFST::complement(const EST_WFST &a)
{
    int i;

    copy(a);  

    for (i=0; i < num_states(); i++)
    {
	if (p_states[i]->type() == wfst_final)
	    p_states[i]->set_type(wfst_nonfinal);
	else if (p_states[i]->type() == wfst_nonfinal)
	    p_states[i]->set_type(wfst_final);
	// errors remain errors
    }
}

static int noloopstostart(const EST_WFST &a)
{
    // TRUE if there are no transitions leading to the start state
    // when this is true there is a union operation which preserves
    // deterministicness
    int i;

    for (i=0; i < a.num_states(); i++)
    {
	const EST_WFST_State *s = a.state(i);
	EST_Litem *p;
	for (p=s->transitions.head(); p != 0; p=p->next())
	{
	    if (s->transitions(p)->state() == a.start_state())
		return FALSE;
	}
    }
    return TRUE;
}

int EST_WFST::deterministiconstartstates(const EST_WFST &a, const EST_WFST &b) const
{
    // TRUE if there are no common transition labels from a and b's
    // start state
    EST_IMatrix tab;
    int in,out;

    tab.resize(a.p_in_symbols.length(),a.p_out_symbols.length());
    tab.fill(0);

    for (EST_Litem *t=a.state(a.start_state())->transitions.head();
	 t != 0; t=t->next())
    {
	tab(a.state(a.start_state())->transitions(t)->in_symbol(),
	    a.state(a.start_state())->transitions(t)->out_symbol()) = 1;
    }

    for (EST_Litem *tt=b.state(b.start_state())->transitions.head();
	 tt != 0; tt=tt->next())
    {
	in = a.in_symbol(b.in_symbol(b.state(b.start_state())->transitions(tt)->in_symbol()));
	out = a.out_symbol(b.out_symbol(b.state(b.start_state())->transitions(tt)->out_symbol()));
	if (in == -1) 
	    continue;  // obviously not a clash
	else if (out == -1) 
	    continue;  // obviously not a clash
	else if (tab(in,out) == 1)
	    return FALSE;
    }
    return TRUE;
}

/***********************************************************************/
/* Build a WFST which accepts both strings of a and of b               */
/***********************************************************************/
void EST_WFST::uunion(const EST_WFST &a,const EST_WFST &b)
{
    EST_IVector smap;
    int i;

    copy(a);
    extend_alphabets(b);

    if (a.deterministic() && b.deterministic() &&
	noloopstostart(a) && noloopstostart(b) &&
	deterministiconstartstates(a,b))
    {
	// This does the union without the epsilon and will preserve
	// deterministic wfsts in this special case
	smap.resize(b.num_states());
	smap[0] = start_state();
	for (i=1; i < b.num_states(); ++i)
	    smap[i] = i+a.num_states()-1;

	more_states(a.num_states()+b.num_states()-1);
	p_num_states += b.num_states()-1;
	for (i=1; i < b.num_states(); i++)
	    p_states[smap(i)] = copy_and_map_states(smap,b.state(i),b);

	const EST_WFST_State *s = b.state(b.start_state());
	EST_Litem *p;
	for (p=s->transitions.head(); p != 0; p=p->next())
	{
	    int mapped_state= smap(s->transitions(p)->state());
	    if (mapped_state != WFST_ERROR_STATE)
		p_states[start_state()]->
		 add_transition(s->transitions(p)->weight(),
		   mapped_state,
		   in_symbol(b.in_symbol(s->transitions(p)->in_symbol())),
		   out_symbol(b.out_symbol(s->transitions(p)->out_symbol())));
	}
    }
    else
    {   // do it the hard way
	smap.resize(b.num_states());
	for (i=0; i < b.num_states(); ++i)
	    smap[i] = i+a.num_states();

	more_states(a.num_states()+b.num_states());
	p_num_states += b.num_states();
	for (i=0; i < b.num_states(); i++)
	    p_states[smap(i)] = copy_and_map_states(smap,b.state(i),b);

	// Actually do the union bit by adding an epsilon transition from
	// the start of a to start state of b
	p_states[start_state()]->add_transition(0.0,smap[b.start_state()],
						in_epsilon(), out_epsilon());
    }

}

/***********************************************************************/
/* Build a WFST which a followed by b                                  */
/***********************************************************************/
void EST_WFST::concat(const EST_WFST &a,const EST_WFST &b)
{
    EST_IVector smap;
    int i;

    copy(a);  
    extend_alphabets(b);

    smap.resize(b.num_states());
    for (i=0; i < b.num_states(); ++i)
	smap[i] = i+a.num_states();

    more_states(a.num_states()+b.num_states());

    // everything final in a becomes non final and an epsilon transition
    // goes from them to the start of b
    for (i=0; i < num_states(); i++)
    {
	if (p_states[i]->type() == wfst_final)
	{
	    p_states[i]->set_type(wfst_nonfinal);
	    p_states[i]->add_transition(0.0,smap[b.start_state()],
					in_epsilon(), out_epsilon());
	}
    }

    p_num_states += b.num_states();
    for (i=0; i < b.num_states(); i++)
	p_states[smap(i)] = copy_and_map_states(smap,b.state(i),b);

}

/***********************************************************************/
/* Build a WFST from composing a and b (feeding output of a to         */
/* input of b)                                                         */
/***********************************************************************/
void EST_WFST::compose(const EST_WFST &a,const EST_WFST &b)
{
    EST_WFST_MultiState *start_state = new EST_WFST_MultiState(wfst_ms_list);
    EST_WFST_MultiState *nms,*current;
    Agenda multistate_agenda;
    EST_WFST_MultiStateIndex index(100);
    wfst_list wl;
    EST_WFST t;
    int i,new_name;
    EST_Litem *p,*q;

    clear();
    p_in_symbols.copy(a.p_in_symbols);
    p_out_symbols.copy(b.p_out_symbols);

    // Unfortunately need to needlessly copy a and b here
    wl.append(a);
    start_state->add(a.start_state());
    wl.append(b);
    start_state->add(b.start_state());

    p_start_state = add_state(intersect_state_type(wl,start_state));
    // Label multistate start start with single-state name
    start_state->set_name(p_start_state);

    multistate_agenda.append(start_state);  // initialize agenda

    while (multistate_agenda.length() > 0)
    {
	current = multistate_agenda.first();
	multistate_agenda.remove(multistate_agenda.head());

	// For all possible in/out pairs
	for (i=0; i < p_in_symbols.length(); i++)
	{   // start at 2 to skip any and epsilon characters -- hmm bad
	    // find transitions
	    wfst_translist transa;

	    wl.first().transduce(current->first(),i,transa);
	    for (p=transa.head(); p != 0; p=p->next())
	    {
		wfst_translist transb;
		// feed a's out to b'is in
		wl.last().transduce(
                          current->last(),
			  b.in_symbol(a.out_symbol(transa(p)->out_symbol())),
			  transb);
		for (q = transb.head(); q != 0; q=q->next())
		{
		    nms = new EST_WFST_MultiState(wfst_ms_list);
		    nms->add(transa(p)->state());
		    nms->add(transb(q)->state());
		    
		    if (intersect_state_type(wl,nms) == wfst_error)
		    {
			delete nms;
			continue;    // no state to go to
		    }
		    new_name = multistate_index(index,nms,p_num_states);
		    if (new_name == p_num_states)  // genuinely new and unseen
		    {   // create a real state and add it to the agenda
			int ns = add_state(intersect_state_type(wl,nms));
			nms->set_name(ns);
			multistate_agenda.append(nms);  
		    }
		    else  // already seen this state, and is already named
			nms->set_name(new_name);

		    // Add new transition to current state
		    p_states[current->name()]
			->add_transition(nms->weight(),nms->name(),
					 i,transb(q)->out_symbol());
		    
		}
		
	    }
	}
	delete current;
	// Probably want some progress summary
    }

}

/***********************************************************************/
/* Build a WFST which accepts strings in a but not in b                */
/***********************************************************************/
void EST_WFST::difference(const EST_WFST &a,const EST_WFST &b)
{
    EST_WFST compb;

    // This is sort of a complement, but not quite
    // But what would the name of this operation be?
    compb.copy(b);
    for (int i=0; i < compb.num_states(); i++)
    {
	if (compb.p_states[i]->type() == wfst_final)
	    compb.p_states[i]->set_type(wfst_error);
    }

    uunion(a,compb);
}

/***********************************************************************/
/* Remove states from which a final state can't be reached             */
/***********************************************************************/
void EST_WFST::remove_error_states(const EST_WFST &a)
{
    // Find all states which are reachable from the start state, and
    // can reach a final state.  Mark all others as error states
    wfst_list wl;

    wl.append(a);
    EST_WFST &ab = wl.first();

    ab.current_tag = ++traverse_tag;
    for (int i=0; i < ab.p_num_states; i++)
	ab.can_reach_final(i);
    // This will copy only the non-error states
    intersection(wl);

}

int EST_WFST::can_reach_final(int state)
{
    // Return TRUE iff this state is final or can reach a final state

    if (p_states[state]->type() == wfst_final)
	return TRUE;
    else if (p_states[state]->type() == wfst_error)
	return FALSE;
    else if (p_states[state]->tag() == current_tag)  
	// Been here and it is reachable
	return TRUE;
    else
    {
	EST_Litem *i;
	enum wfst_state_type current_val = p_states[state]->type();
	enum wfst_state_type r = wfst_error;
	// temporarily set this to error to stop infinite recursion
	p_states[state]->set_type(wfst_error);

	for (i=p_states[state]->transitions.head(); i != 0; i=i->next())
	    // if any transition goes to something that reaches a final state
	    // set the value back to its original
	    if (can_reach_final(p_states[state]->transitions(i)->state()))
		r = current_val;

	// Will be set back to original value iff a final state
	// is reachable from here
	p_states[state]->set_type(r);
	if (r == wfst_error)
	    return FALSE;
	else
	{
	    p_states[state]->set_tag(current_tag);
	    return TRUE;
	}
    }
}

/***********************************************************************/
/* True is wfst is deterministic                                       */
/***********************************************************************/
int EST_WFST::deterministic() const
{
    // True if all states contains no multiple arcs with the same symbol
    EST_IMatrix tab;

    tab.resize(p_in_symbols.length(),p_out_symbols.length());

    for (int i=0; i < p_num_states; i++)
    {
	tab.fill(0);
	for (EST_Litem *t=state(i)->transitions.head(); t != 0; t=t->next())
	{
	    if (tab(state(i)->transitions(t)->in_symbol(),
		    state(i)->transitions(t)->out_symbol()) == 1)
		return FALSE;
	    else
		tab(state(i)->transitions(t)->in_symbol(),
		    state(i)->transitions(t)->out_symbol()) = 1;
	}
    }
    return TRUE;
}
