/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1999                            */
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
/*                     Date   :  September 1999                          */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Building tree lexicons from rules.  This could almost be done by the  */
/* regular grammar compiler but for efficiently reasons we have a        */
/* specific compiler.  As the ends of the trees are never minimized      */
/* Actually it will take full context free grammars and convert them     */
/* up to a specified rewrite depth                                       */
/*                                                                       */
/* Based loosely on "Finite State Machines from Features Grammars" by    */
/* Black, International Workshop of Parsing Technologies, CMU 89         */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include "EST_cutils.h"
#include "EST_THash.h"
#include "EST_WFST.h"

static LISP tl_find_l_w(LISP rules);
static int production_index(LISP state,
			    EST_TStringHash<int> &index,
			    int proposed);

void tlcompile(LISP tl, EST_WFST &all_wfst)
{
    // Build a transducer from given tree-lexicon.  
    LISP l_w,letters,words;
//    LISP sets=siod_nth(2,tl);
    LISP rules=siod_nth(3,tl);

    l_w = tl_find_l_w(rules);
    letters = car(l_w);  // or phones or "bits" or whatever
    words = cdr(l_w);    // the things you are indexing

    all_wfst.build_tree_lex(letters,words,rules);

}

static LISP tl_find_l_w(LISP rules)
{
    // Find the alphabets used in the rules
    LISP letters=NIL,words=NIL,r,s;
    
    for (r=rules; r != NIL; r=cdr(r))
    {
	for (s = car(r); s != NIL; s=cdr(s))
	{
	    if (streq("->",get_c_string(car(s))))
	    {
		if (!siod_member_str(get_c_string(car(cdr(s))),words))
		    words = cons(car(cdr(s)),words);
		break;
	    }
	    else if (!siod_member_str(get_c_string(car(s)),letters))
	    {
		letters = cons(car(s),letters);
	    }
	}
    }
	
    return cons(letters,words);
}


void EST_WFST::build_tree_lex(LISP inalpha, LISP outalpha, 
			      LISP wlist)
{
    // Build a determinized tree lexicon for given list
    LISP w,l;
    int cs,ns;
    EST_WFST_Transition *trans;
    EST_TStringHash<int> index(100);
    int fff;

    clear();
    init(inalpha,outalpha);
    int i_epsilon = in_epsilon();
    int o_epsilon = out_epsilon();
    float weight;

    // Create a starting state and add it to this WFST
    p_start_state = add_state(wfst_nonfinal);
    fff = add_state(wfst_final);

    for (w=wlist; w; w=cdr(w))
    {
//	lprint(car(w));
	weight = get_c_float(car(siod_last(car(w))));
	for (cs=p_start_state,l=car(w); l; l=cdr(l))
	{
	    if (streq("->",get_c_string(car(l))))
	    {
		// reached word
		trans = find_transition(cs,i_epsilon,
			  p_out_symbols.name(get_c_string(car(cdr(l)))));
		if (trans == 0)
		{
		    p_states[cs]
			->add_transition(weight,
			   fff,i_epsilon,
			   p_out_symbols.name(get_c_string(car(cdr(l)))));
		}
//		else // duplicate word
//		{
//		    cerr << "WFST: tlcompile, duplicate word ignored\n";
//		}
		break;
	    }
	    else
	    {
		trans = find_transition(cs,
			 p_in_symbols.name(get_c_string(car(l))),
 		         o_epsilon);
		if (trans == 0)
		{
		    ns = production_index(cdr(l),index,p_num_states);
		    if (ns == p_num_states)
			ns = add_state(wfst_nonfinal);
		    p_states[cs]
			->add_transition(weight,
			   ns,
			   p_in_symbols.name(get_c_string(car(l))),
			   o_epsilon);
		}
		else // increment count
		{
		    ns = trans->state();
		    trans->set_weight(trans->weight()+weight);
		}
	    }
	    cs = ns;
	}
    }

    // normalized transition weights into probabilities
    stop_cumulate();
}

static int production_index(LISP state,
			    EST_TStringHash<int> &index,
			    int proposed)
{
    // Returns proposed if ms is not already in index, otherwise
    // returns the value that was proposed when it was first new.

    // I'll have to make this more efficient in future.
    EST_String istring("");
    LISP p;
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
