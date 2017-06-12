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
/*                     Author :  Alan W Black                            */
/*                     Date   :  December 1997                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* A Regular grammar compiler, its pretty free about the grammar         */
/* Actually it will take full context free grammars and convert them     */
/* up to a specified rewrite depth                                       */
/*                                                                       */
/* Based loosely on "Finite State Machines from Features Grammars" by    */
/* Black, International Workshop of Parsing Technologies, CMU 89         */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include "EST_cutils.h"
#include "EST_WFST.h"

static LISP find_rewrites(LISP rules, LISP terms, LISP nonterms);
static LISP rg_find_nt_ts(LISP rules,LISP sets);
static LISP prod_join(LISP n, LISP p);
static int production_index(LISP state,
			    EST_WFST_MultiStateIndex &index,
			    int proposed);

void rgcompile(LISP rg, EST_WFST &all_wfst)
{
    // Build a transducer from given regular grammar.  
    LISP nt_ts,nonterms,terms,rewrites;
    LISP sets=siod_nth(2,rg);
    LISP rules=siod_nth(3,rg);

    nt_ts = rg_find_nt_ts(rules,sets);
    nonterms = car(nt_ts);
    terms = cdr(nt_ts);

    rewrites = find_rewrites(rules,terms,nonterms);

    if (rewrites == NIL)
	return;         // left recursive or no rules
    
    all_wfst.build_from_rg(terms,terms,
			   car(car(rules)),  // distinguished symbol 
			   rewrites,
			   sets,terms,25);  

}

static LISP find_rewrites(LISP rules, LISP terms, LISP nonterms)
{
    // Find the full rewrites of each nonterminal until a terminal
    // appears as the first item
    LISP nt,r;
    LISP rewrites = NIL;
    (void)terms;

    // got lazy and haven't done this recursively yet ... 
    for (nt=nonterms; nt != NIL; nt=cdr(nt))
    {
	LISP nn = NIL;
	for (r=rules; r != NIL; r=cdr(r))
	    if (car(car(r)) == car(nt))  // depend on symbols being eq
		nn = cons(cdr(cdr(car(r))),nn);
	rewrites = cons(cons(car(nt),nn),rewrites);
    }

    return rewrites;
}

static LISP rg_find_nt_ts(LISP rules,LISP sets)
{
    // Find the alphabets used in the rules
    LISP terms=NIL,nonterms=NIL,r,s,set,t;
    
    for (r=rules; r != NIL; r=cdr(r))
	if (!siod_member_str(get_c_string(car(car(r))),nonterms))
	    nonterms = cons(car(car(r)),nonterms);

    for (r=rules; r != NIL; r=cdr(r))
	for (s=cdr(cdr(car(r))); s != NIL; s=cdr(s))
	    if ((!siod_member_str(get_c_string(car(s)),terms)) &&
		(!siod_member_str(get_c_string(car(s)),nonterms)) &&
		(!siod_assoc_str(get_c_string(car(s)),sets)))
		terms = cons(car(s),terms);
	    else if ((set=siod_assoc_str(get_c_string(car(s)),sets)))
	    {
		for (t=cdr(set); t != 0; t=cdr(t))
		    if (!siod_member_str(get_c_string(car(t)),terms))
			terms = cons(car(t),terms);
	    }
	
    return cons(nonterms,terms);
}


void EST_WFST::build_from_rg(LISP inalpha, LISP outalpha, 
			     LISP distinguished, LISP rewrites,
			     LISP sets, LISP terms,
			     int max_depth)
{
    // This is sort of similar to determinising in that the "state"
    // is represented by a list of numbers, i.e. the remainder of
    // of production
    LISP current, start_state, remainder, set, new_prod;
    int ns, current_state;
    const char *current_sym;
    LISP agenda = NIL;
    EST_WFST_MultiStateIndex index(100);
    (void)max_depth;
    int c=0;

    clear();
    init(inalpha,outalpha);
    int i_epsilon = in_epsilon();
    int o_epsilon = out_epsilon();

    // Create a starting state and add it to this WFST
    p_start_state = add_state(wfst_nonfinal);
    start_state = cons(flocons((double)p_start_state),
		       cons(distinguished,NIL));
    
    production_index(start_state,index,p_start_state);

    agenda = cons(start_state,agenda); // initialize agenda

    while (agenda != NIL)
    {
	current = car(agenda);
	agenda = cdr(agenda);
	current_state = get_c_int(car(current));
	current_sym = get_c_string(car(cdr(current)));
	remainder = cdr(cdr(current));
	if ((c % 1000)== 0)
	    cout << summary() << " Agenda " << siod_llength(agenda) << endl;
	c++;

	if ((set=siod_assoc_str(current_sym,sets)))
	{
	    ns = production_index(remainder,index,p_num_states);
	    // add transitions for each member of set
	    for (LISP s=cdr(set); s != NIL; s=cdr(s))
		p_states[current_state]
		    ->add_transition(0.0, // no weights
				     ns,
				     p_in_symbols.name(get_c_string(car(s))),
				     p_out_symbols.name(get_c_string(car(s))));
	    if (remainder == NIL)
		add_state(wfst_final);
	    else if (ns == p_num_states)  // its a new remainder
	    {
		add_state(wfst_nonfinal);
		agenda = cons(cons(flocons(ns),remainder),agenda);
	    }
	}
	else if (siod_member_str(current_sym,terms))
	{
	    ns = production_index(remainder,index,p_num_states);
	    // Add transition for this terminal symbol
	    p_states[current_state]
		->add_transition(0.0, // no weights
				 ns,
				 p_in_symbols.name(current_sym),
				 p_out_symbols.name(current_sym));
	    if (remainder == NIL)
		add_state(wfst_final);
	    else if (ns == p_num_states)  // its a new remainder
	    {
		add_state(wfst_nonfinal);
		agenda = cons(cons(flocons(ns),remainder),agenda);
	    }
	}
	else // its a non-terminal so simply rewrite
	{
	    for (LISP p=cdr(siod_assoc_str(current_sym,rewrites));
		 p != NIL; 
		 p=cdr(p))
	    {
		new_prod = prod_join(car(p),remainder);
		ns = production_index(new_prod,index,p_num_states);
		p_states[current_state]
		    ->add_transition(0.0, // no weights
				     ns,i_epsilon,o_epsilon);
		if (ns == p_num_states)  // its a new remainder
		{
		    if (new_prod == NIL)
			add_state(wfst_final);
		    else
		    {
			add_state(wfst_nonfinal);
			agenda = cons(cons(flocons(ns),new_prod),agenda);
		    }
		}
	    }
	}
    }
}

static int production_index(LISP state,
			    EST_WFST_MultiStateIndex &index,
			    int proposed)
{
    // Returns proposed if ms is not already in index, otherwise
    // returns the value that was proposed when it was first new.

    // I'll have to make this more efficient in future.
    EST_String istring("");
    LISP p;

    for (p=state; p != NIL; p = cdr(p))
	istring += EST_String(get_c_string(car(p))) + " ";

    int ns,found;

    for (p=state; p != NIL; p = cdr(p))
	istring += EST_String(get_c_string(car(p))) + " ";

    ns = index.val(istring,found);
    if (found)
	return ns;
    else
    {
        index.add_item(istring,proposed);
	return proposed;
    }

}

static LISP prod_join(LISP n, LISP p)
{
    if (n == NIL)
	return p;
    else 
	return cons(car(n),prod_join(cdr(n),p));
}
