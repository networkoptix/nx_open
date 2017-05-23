/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                      Copyright (c) 1996-1998                          */
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
/* A Koskenniemi/Kay/Kaplan rule compiler to WFST using the techniques   */
/* Ritchie et al.'s "Computational Morphology" (but followed through to  */
/* make real WFSTs).                                                     */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include "EST_WFST.h"
#include "EST_cutils.h"

ostream &operator << (ostream &s, const EST_WFST &w)
{
  (void)w;
  return s << "<<EST_WFST>>";
}

Declare_TList(EST_WFST)

#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_TList.cc"

Instantiate_TList(EST_WFST)

#endif

static LISP expand_fp(const EST_String p,LISP fp);
static LISP find_feasible_pairs(LISP rules);
static LISP all_but(LISP rulepair,LISP fp);
static LISP expand_sets(LISP sets,LISP fp);
static LISP inline_sets(LISP l, LISP sets);
static void full_kkcompile(LISP inalpha,LISP outalpha,
			   LISP fp, LISP rules, LISP sets,
			   EST_WFST &all_wfst);

void kkcompile(LISP ruleset, EST_WFST &all_wfst)
{
    // Build a transducer from given kkrule (Kay/Kaplan/Koskenniemi)
    // Rules are of the form LeftContext Map RightContext

    // The WFST is recognizing all string except the rulepair unless
    // its in the proper context.
    LISP fp;  // feasible pairs, those pairs with rules (rather than IxO)
    LISP inalpha = siod_nth(1,siod_assoc_str("Alphabets",cdr(cdr(ruleset))));
    LISP outalpha = siod_nth(2,siod_assoc_str("Alphabets",cdr(cdr(ruleset))));
    LISP sets = cdr(siod_assoc_str("Sets",ruleset));
    LISP rules = cdr(siod_assoc_str("Rules",ruleset));

    fp = find_feasible_pairs(rules);
    sets = expand_sets(sets,fp);

    full_kkcompile(inalpha,outalpha,fp,rules,sets,all_wfst);
}

static void full_kkcompile(LISP inalpha,LISP outalpha,
			   LISP fp, LISP rules, LISP sets,
			   EST_WFST &all_wfst)
{
    wfst_list rulelist;
    LISP r;

    for (r=rules; r != NIL; r=cdr(r))
    {
	EST_WFST r_wfst,base_wfst,det_wfst;
	rulelist.append(r_wfst);
	EST_WFST &rr_wfst = rulelist.last(); // to avoid copying the filled one
	cout << "Rule: " << siod_llength(rules)-siod_llength(r) << endl;
	pprint(car(r));
	base_wfst.kkrule_compile(inalpha,outalpha,fp,car(r),sets);
	cout << "          base " << base_wfst.summary() << endl;
	det_wfst.determinize(base_wfst);
	cout << "  determinized " << det_wfst.summary() << endl;
	rr_wfst.minimize(det_wfst);
	cout << "     minimized " << rr_wfst.summary() << endl;
    }

    cout << "WFST: intersecting " << rulelist.length() << " rules" << endl;
    EST_Litem *p,*nnp;
    int i;
    for (i=0,p=rulelist.head(); p->next() != 0; p=nnp)
    {
	EST_WFST r_wfst,base_wfst,det_wfst;
	EST_WFST mmm;
	rulelist.append(r_wfst);
	EST_WFST &rr_wfst = rulelist.last(); // to avoid copying the filled one
	cout << "intersecting " << i << " and " << i+1 << " " <<
	    rulelist.length()-2 << " left" << endl;
	cout << "   " << rulelist(p).summary() << " and " << endl;
	cout << "   " << rulelist(p->next()).summary() << " becomes " << endl;
	mmm.intersection(rulelist(p),rulelist(p->next()));
	cout << "   " << mmm.summary() << " minimizes to " << endl;
	rr_wfst.minimize(mmm);
	cout << "   " << rr_wfst.summary() << endl;
	nnp=p->next()->next();
	i+=2;
	rulelist.remove(p->next());
	rulelist.remove(p);
    }

    all_wfst = rulelist.first();

}

static LISP expand_sets(LISP sets,LISP fp)
{
    // Expand sets into regexes that represent them.  Single
    // char values are converted to disjunctions of feasible pairs
    // that have the same surface character
    LISP s,es=NIL,e,ne;

    for (s=sets; s != NIL; s=cdr(s))
    {
	for (ne=NIL,e=cdr(car(s)); e != NIL; e=cdr(e))
	{
	    EST_String ss = get_c_string(car(e));
	    if (ss.contains("/"))
		ne = cons(car(e),ne);
	    else
		ne = append(expand_fp(ss,fp),ne);
	}
	if (ne == NIL)
	{
	    cerr << "WFST: kkcompile: set " << get_c_string(car(car(s))) <<
		" has no feasible pairs" << endl;
	}

	else if (siod_llength(ne) == 1)
	    es = cons(cons(car(car(s)),ne),es);
	else
	    es = cons(cons(car(car(s)),
			   cons(cons(rintern("or"),reverse(ne)),
				NIL)),es);
    }

    return reverse(es);
}

static LISP expand_fp(const EST_String p,LISP fp)
{
    // Find all fp's that have this p as their surface char
    LISP m=NIL,f;
    EST_Regex rg(EST_String("^")+p+"/.*");

    for (f=fp; f != NIL; f=cdr(f))
    {
	EST_String ss = get_c_string(car(f));
	if ((p == ss) || (ss.matches(rg)))
	    m = cons(car(f),m);
    }
    return m;
}

static LISP find_feasible_pairs(LISP rules)
{
    // Find the set of pairs that have rules associated with them
    // This effectively defines the transducer alphabet.
    LISP fp = NIL;
    LISP r;

    for (r=rules; r != NIL; r=cdr(r))
    {
	if (siod_member_str(get_c_string(siod_nth(0,car(r))),fp) == NIL)
	    fp = cons(siod_nth(0,car(r)),fp);
    }
    return fp;
}

static int surface_coercion(LISP rt)
{
    return (streq("<=",get_c_string(rt)));
}

static int context_restriction(LISP rt)
{
    return (streq("=>",get_c_string(rt)));
}

static int composite(LISP rt)
{
    return (streq("<=>",get_c_string(rt)));
}

static LISP inline_sets(LISP l, LISP sets)
{
    // Replace any set name with the regex equivalent
    LISP s;
    if (l == NIL)
	return NIL;
    else if (consp(l))
	return cons(inline_sets(car(l),sets),inline_sets(cdr(l),sets));
    else if ((s=siod_assoc_str(get_c_string(l),sets)) != NIL)
	return car(cdr(s));
    else
	return l;
}

void EST_WFST::kkrule_compile(LISP inalpha, LISP outalpha, LISP fp, 
			      LISP rule,LISP sets)
{
    // Build a WFST to transduce this particular rule
    // Accepts any other combination of feasible pairs too
    LISP leftcontext = inline_sets(siod_nth(2,rule),sets);
    LISP rulepair = siod_nth(0,rule);
    LISP ruletype = siod_nth(1,rule);
    LISP rightcontext = inline_sets(siod_nth(4,rule),sets);
    LISP p;
    int i;
    int end_LC,end_RP,end_NOTRP,end_RC,err_state;

    // Initialize alphabets
    init(inalpha,outalpha);  // should be passed as discretes

    p_start_state = add_state(wfst_final);  // empty WFST
    // Add transitions for all pairs except rulepair
    for (p=fp; p != NIL; p=cdr(p))
	if ((!equal(rulepair,car(p))) ||
	    (surface_coercion(ruletype)))
	    build_wfst(p_start_state,p_start_state,car(p));

    // build for LC 
    if (leftcontext)
    {
	end_LC = add_state(wfst_final);
	build_wfst(p_start_state,end_LC,leftcontext);
	// for all states in LC mark final & add epsilon to p_start_state
	for (i=end_LC; i < p_num_states; i++)
	{
	    build_wfst(i,p_start_state,epsilon_label());
	    p_states[i]->set_type(wfst_final);
	}
    }
    else   // no LC
	end_LC = p_start_state;

    // build for RP and RC from end_LC
    if (composite(ruletype) || context_restriction(ruletype))
    {
	if (rightcontext)
	{
	    end_RP = add_state(wfst_nonfinal);
	    build_wfst(end_LC,end_RP,rulepair);
	    // build for RC from end map to p_start_state
	    build_wfst(end_RP,p_start_state,rightcontext);
	    err_state = add_state(wfst_error);
	    for (i=end_RP; i < err_state; i++)
	    {   // for everything other that the correct path go to err_state
		// without this explicit error state the epsilon to start
		// allows almost everything
		if (transition(i,get_c_string(epsilon_label())) 
		    != WFST_ERROR_STATE)
		    break;  // not a state require extra transitions
		for (p=fp; p != NIL; p=cdr(p))
		    if (transition(i,get_c_string(car(p))) == WFST_ERROR_STATE)
			build_wfst(i,err_state,car(p));
		build_wfst(i,p_start_state,epsilon_label());
		p_states[i]->set_type(wfst_licence);
	    }
	}
	else  // no RC, so end back at start
	    build_wfst(end_LC,p_start_state,rulepair);
    }

    // Build for notRP and RC from end_LC
    if (composite(ruletype) || surface_coercion(ruletype))
    {
	LISP abrp = all_but(rulepair,fp);
	if (abrp)
	{
	    if (rightcontext)
	    {
		end_RC = add_state(wfst_error);
		end_NOTRP = add_state(wfst_nonfinal);
		build_wfst(end_LC,end_NOTRP,abrp);
		// build for RC from end RP to error state
		build_wfst(end_NOTRP,end_RC,rightcontext);
		// for all states in RC except final one mark final & add 
		// epsilon to p_start_state
		for (i=end_NOTRP; i < p_num_states; i++)
		{
		    build_wfst(i,p_start_state,epsilon_label());
		    p_states[i]->set_type(wfst_final);
		}
	    }
	    else			// no RC, 
	    {
		end_RC = add_state(wfst_error);
		build_wfst(end_LC,end_RC,abrp);
	    }
	}
    }
}

static LISP all_but(LISP rulepair,LISP fp)
{
    // Returns pairs that have the same surface symbol as rulepair
    // but different lexical symbol
    LISP r,notrp=NIL;
    EST_String s,l,p,sr,lr,rr;
    
    p = get_c_string(rulepair);
    if (p.contains("/"))
    {
	s = p.before("/");
	l = p.after("/");
    }
    else
    {
	s = p;
	l = p;
    }
    
    for (r=fp; r != NIL; r = cdr(r))
    {
	rr = get_c_string(car(r));
	if (rr.contains("/"))
	{
	    sr = rr.before("/");
	    lr = rr.after("/");
	}
	else
	{
	    sr = rr;
	    lr = rr;
	}
	if ((l != lr) && (s == sr))
	    notrp = cons(car(r),notrp);
    }

    if (siod_llength(notrp) > 1)
	notrp = cons(strintern("or"),notrp);
    return notrp;
}

void intersect(wfst_list &wl, EST_WFST &all)
{
    // Intersect the wfst's in wl into all
    
    all.intersection(wl);
    
}

