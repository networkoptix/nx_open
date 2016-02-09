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
/* WFST functions for building from REGEXs                               */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include "EST_cutils.h"
#include "EST_WFST.h"

void EST_WFST::build_or_transition(int start, int end, LISP disjunctions)
{
    // Choice of either disjunct
    LISP l;
    int intermed;

    if (disjunctions == NIL)
	cerr << "WFST construct: disjunct is nil\n";

    for (l=disjunctions; l != NIL; l=cdr(l))
    {
	// Can't go directly to end as other transitions could be added there
	intermed = add_state(wfst_nonfinal);
	build_wfst(start,intermed,car(l));
	build_wfst(intermed,end,epsilon_label());
    }
}

void EST_WFST::build_and_transition(int start, int end, LISP conjunctions)
{
    // require each conjunct in turn
    int intermed,lstart;
    LISP l;

    if (conjunctions == NIL)
	cerr << "WFST build: conjunct is nil\n";

    lstart = start;
    for (l=conjunctions; cdr(l) != NIL; l=cdr(l))
    {
	intermed = add_state(wfst_nonfinal);
	build_wfst(lstart,intermed,car(l));
	lstart = intermed;
    }
    build_wfst(lstart,end,car(l));
    
}

int EST_WFST::terminal(LISP l)
{
    // true, l is a terminal in a regex

    if (atomp(l))
	return TRUE;
    else
	return FALSE;
}

int EST_WFST::operator_or(LISP l)
{
    if (l && !consp(l) && (streq("or",get_c_string(l))))
	return TRUE;
    else
	return FALSE;
}

int EST_WFST::operator_plus(LISP l)
{
    if (l && !consp(l) && (streq("+",get_c_string(l))))
	return TRUE;
    else
	return FALSE;
}

int EST_WFST::operator_not(LISP l)
{
    if (l && !consp(l) && (streq("not",get_c_string(l))))
	return TRUE;
    else
	return FALSE;
}

int EST_WFST::operator_star(LISP l)
{
    if (l && !consp(l) && (streq("*",get_c_string(l))))
	return TRUE;
    else
	return FALSE;
}

int EST_WFST::operator_optional(LISP l)
{
    if (l && !consp(l) && (streq("?",get_c_string(l))))
	return TRUE;
    else
	return FALSE;
}

int EST_WFST::operator_and(LISP l)
{
    if (l && !consp(l) && (streq("and",get_c_string(l))))
	return TRUE;
    else
	return FALSE;
}

void EST_WFST::build_wfst(int start, int end,LISP regex)
{
    if (terminal(regex))
    {
	// unpack the label
	int in,out;
	EST_String s_name(get_c_string(regex));
	if (s_name.contains("/"))
	{
	    in = p_in_symbols.name(s_name.before("/"));
	    out = p_out_symbols.name(s_name.after("/"));
	}
	else
	{
	    in = p_in_symbols.name(get_c_string(regex));
	    out = p_out_symbols.name(get_c_string(regex));
	}
	if ((in == -1) || (out == -1))
	    cerr << "WFST_build: symbol " <<  get_c_string(regex) <<
			   " not in alphabet\n";
	p_states[start]->add_transition(0,end,in,out);
    }
    else if (operator_or(car(regex)))
	build_or_transition(start,end,cdr(regex));
    else if (operator_plus(car(regex)))
    {
	build_wfst(start,end,cdr(regex));
	build_wfst(end,end,cdr(regex));
    }
    else if (operator_star(car(regex)))
    {
	build_wfst(start,start,cdr(regex));
	build_wfst(start,end,epsilon_label());
    }
    else if (operator_not(car(regex)))
    {
	int errstate = add_state(wfst_error);
	build_and_transition(start,errstate,cdr(regex));
    }
    else if (operator_optional(car(regex)))
    {
	build_wfst(start,end,cdr(regex));
	build_wfst(start,end,epsilon_label());
    }
    else if (operator_and(car(regex))) 
	build_and_transition(start,end,cdr(regex));
    else
	build_and_transition(start,end,regex);  // default is and
}

void EST_WFST::build_from_regex(LISP inalpha, LISP outalpha, LISP regex)
{

    clear();

    cout << "building from regex: " << endl;
    pprint(regex);

    init(inalpha,outalpha);   // alphabets
    if (regex == NIL)
	p_start_state = add_state(wfst_final);  // empty WFST
    else 
    {
	p_start_state = add_state(wfst_nonfinal);
	int end = add_state(wfst_final);
	build_wfst(p_start_state,end,regex);
    }
}

