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
/* A LTS rule compiler, where rules are contextual rewrite rules.  Rules */
/* are of the for LC [ x ] RC => y where LC and RC are regexs on the     */
/* tape only and x and y are simple strings on symbols.  That is the     */
/* standard form of LTS rules used in Festival.                          */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include "EST_cutils.h"
#include "EST_WFST.h"

static LISP lts_find_feasible_pairs(LISP rules);
static LISP make_fp(LISP in, LISP out);
static LISP find_outs(LISP rule);
static LISP find_ins(LISP rule);
static LISP add_alpha(LISP n, LISP existing);
static LISP lts_find_alphabets(LISP rules);
static void ltsrule_compile(LISP inalpha, LISP outalpha, 
			    LISP fp, LISP sets, LISP rule,
			    EST_WFST &a, EST_WFST &not_a);
static LISP analyse_rule(LISP rule);
static LISP expand_sets(LISP l, LISP fp, LISP sets);
static LISP expand_set(LISP p, LISP fp, LISP sets);
static LISP find_notMAP(LISP MAP,LISP fp);

void ltscompile(LISP lts_rules, EST_WFST &all_wfst)
{
    // Build a transducer from given LTS rules.  Because the interpretation
    // of these rules is normally ordered and the WFST is not, the
    // complement of each cumulative WFST must be generated before 
    // adding the next rule
    LISP r;
    LISP fp;  // feasible pairs, those pairs with rules (rather than IxO)
    LISP inalpha,outalpha,alphas;
    LISP sets=siod_nth(2,lts_rules);
    LISP rules=siod_nth(3,lts_rules);
    EST_WFST nots;

    alphas = lts_find_alphabets(lts_rules);
    inalpha = car(alphas);
    outalpha = cdr(alphas);
    
    fp = lts_find_feasible_pairs(rules);

    // set up an empty WFST, accepts nothing
    all_wfst.build_from_regex(inalpha,outalpha,NIL);  
    // matches things not matched by rules, everything to begin with
    nots.build_from_regex(inalpha,outalpha,
			  cons(rintern("*"),
			       cons(cons(rintern("or"),fp),NIL)));
    nots.save("-");

    for (r=rules; r != NIL; r=cdr(r))
    {
	EST_WFST a, not_a,b,c,d;

	// all = all u (all' n r)
	ltsrule_compile(inalpha,outalpha,fp,sets,car(r),a,not_a);
	pprint(car(r));
	a.save("-");
	c.intersection(a,nots);
	c.save("-");

	// Add for next rule
	b.uunion(nots,not_a);
	not_a.save("-");
	b.save("-");
	nots = b;

	d.uunion(all_wfst,c);
	all_wfst = d;
	all_wfst.save("-");
    }
}

static LISP lts_find_alphabets(LISP rules)
{
    // Find the alphabets used in the rules
    LISP r;
    LISP in=NIL, out=NIL;
    
    for (r=siod_nth(3,rules); r != NIL; r=cdr(r))
    {
	in = add_alpha(find_ins(car(r)),in);
	out = add_alpha(find_outs(car(r)),out);
    }

    return cons(in,out);
}

static LISP add_alpha(LISP n, LISP existing)
{
    // Add values in n if not already in existing
    LISP t;
    LISP e=existing;

    for (t=n; t != NIL; t=cdr(t))
	if (!siod_member_str(get_c_string(car(t)),e))
	    e = cons(car(t),e);
    
    return e;
}

static LISP find_ins(LISP rule)
{
    // find all symbols in [] in rule
    LISP c;
    int state=FALSE;
    LISP ins = NIL;

    for (c=rule; c != NIL; c=cdr(c))
    {
	if (streq("[",get_c_string(car(c))))
	    state=TRUE;
	else if (streq("]",get_c_string(car(c))))
	    break;
	else if (state)
	    ins = cons(car(c),ins);
    }
    return reverse(ins);
}

static LISP find_outs(LISP rule)
{
    // find all symbols after = rule
    LISP c;
    int state=FALSE;
    LISP outs = NIL;

    for (c=rule; c != NIL; c=cdr(c))
    {
	if (streq("=",get_c_string(car(c))))
	    state=TRUE;
	else if (state)
	    outs = cons(car(c),outs);
    }
    return reverse(outs);
}

static LISP lts_find_feasible_pairs(LISP rules)
{
    // Find the set of pairs that have rules associated with them
    // This effectively defines the transducer alphabet.
    // We take the surface part in [] and the part after the = and
    // linearly match them to form a set of pairs, padded with epsilon
    // if necessary.
    LISP fp = NIL;
    LISP r;

    for (r=rules; r != NIL; r=cdr(r))
    {
	LISP in = find_ins(car(r));
	LISP out = find_outs(car(r));

	LISP pairs = make_fp(in,out);
	
	for (LISP p=pairs; p != NIL; p=cdr(p))
	{
	    if (!siod_member_str(get_c_string(car(p)),fp))
		fp = cons(car(p),fp);
	}
    }
    return fp;
}

static LISP make_fp(LISP in, LISP out)
{
    // Returns a list of pairs by matching each member of in to out
    // padding the shorted one with _epsilon_ if necessary
    LISP i,o;
    LISP fp=NIL;
    EST_String is,os;
    int m;
    
    if (siod_llength(in) > siod_llength(out))
	m = siod_llength(in);
    else
	m = siod_llength(out);

    for (i=in,o=out ; m > 0; --m,i=cdr(i),o=cdr(o))
    {
	if (i == NIL)
	    is = "__epsilon__";
	else
	    is = get_c_string(car(i));
	if (o == NIL)
	    os = "__epsilon__";
	else
	    os = get_c_string(car(o));
        fp = cons(strintern(is+"/"+os),fp);
    }
    return reverse(fp);
}

static void ltsrule_compile(LISP inalpha, LISP outalpha, 
			    LISP fp, LISP sets, LISP rule,
			    EST_WFST &a, EST_WFST &not_a)
{
    // Return two regexs, one matching with rewrites and another
    // that matches things this rule doesn't match.
    LISP LC,MAP,RC,notMAP,r;

    r = analyse_rule(rule);
    LC = siod_nth(0,r);
    MAP = siod_nth(1,r);
    RC = siod_nth(2,r);

    LC = expand_sets(LC,fp,sets);
    RC = expand_sets(RC,fp,sets);
    notMAP = find_notMAP(MAP,fp);


    LISP kk = cons(LC,cons(MAP,cons(RC,NIL)));
    cout << "kk rule" << endl;
    pprint(kk);
    a.kkrule_compile(inalpha,outalpha,fp,kk,NIL);
    
    // (or (* <fp>) (not <rule>))  ;;  everything except the rule
    LISP regex_r = cons(rintern("and"),append(LC,append(MAP,RC)));
//    LISP nn = cons(rintern("or"),
//		   cons(cons(rintern("*"),cons(cons(rintern("or"),fp),NIL)),
//			cons(cons(rintern("not"),cons(regex_r,NIL)),
//			     NIL)));
    LISP nn = cons(rintern("not"),cons(regex_r,NIL));
    not_a.build_from_regex(inalpha,outalpha,nn);

}

static LISP analyse_rule(LISP rule)
{
    // return the left context, map and right context;
    LISP LC=NIL, RC=NIL, in=NIL, out=NIL;
    LISP l;
    int state=0;

    for (l=rule; l != NIL; l=cdr(l))
    {
	if ((state==0) && (!streq("[",get_c_string(car(l)))))
	    LC = cons(car(l),LC);
	else if ((state==0) && (streq("[",get_c_string(car(l)))))
	    state = 1;
	else if ((state==1) && (!streq("]",get_c_string(car(l)))))
	    in = cons(car(l),in);
	else if ((state==1) && (streq("]",get_c_string(car(l)))))
	    state = 2;
	else if ((state==2) && (!streq("=",get_c_string(car(l)))))
	    RC = cons(car(l),RC);
	else if ((state==2) && (streq("=",get_c_string(car(l)))))
	    state = 3;
	else if (state == 3)
	{
	    out = l;
	    break;
	}
    }
    
    return cons(reverse(LC),
	    cons(make_fp(reverse(in),out),
	     cons(reverse(RC),NIL)));

}

static LISP expand_sets(LISP l, LISP fp, LISP sets)
{
    // Expand sets in l and fix regex characters 
    LISP r,es=NIL;

    for (r=l; r != NIL; r=cdr(r))
    {
	LISP s = expand_set(car(r),fp,sets);
	if (cdr(r) && (streq("*",get_c_string(car(cdr(r))))))
	{
	    es = cons(cons(rintern("*"),s),es);
	    r=cdr(r);
	}
	else if (cdr(r) && (streq("+",get_c_string(car(cdr(r))))))
	{
	    es = cons(cons(rintern("+"),s),es);
	    r=cdr(r);
	}
	else
	    es = cons(cons(rintern("and"),s),es);
    }
    return reverse(es);
}

static LISP expand_set(LISP p, LISP fp, LISP sets)
{
    // expand p with respect to sets and feasible pairs
    LISP set = siod_assoc_str(get_c_string(p),sets);
    LISP s,f;
    LISP r=NIL;

    if (set == NIL)
	set = cons(p,NIL);

    for (s=set; s != NIL; s=cdr(s))
    {
	for (f=fp; f != NIL; f=cdr(f))
	{
	    EST_String ss = get_c_string(car(s));
	    EST_String sf = get_c_string(car(f));
	    
	    if (sf.contains(ss+"/"))
		r = cons(car(f),r);
	}
    }

    return reverse(r);
}

static LISP find_notMAP(LISP MAP,LISP fp)
{
    // Returns REGEX that matches everything except MAP,  this doesn't
    // try all possible epsilons though 
    LISP r,notrp=NIL,m,np;
    EST_String s,l,p,sr,lr,rr;

    for (m=MAP; m != NIL; m=cdr(m))
    {
	p = get_c_string(car(m));
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

	for (np=NIL,r=fp; r != NIL; r = cdr(r))
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
	    if ((s == sr) && (l != lr))
		np = cons(car(r),np);
	}
	notrp = cons(cons(rintern("or"),np),notrp);
    }

    return reverse(notrp);
}

