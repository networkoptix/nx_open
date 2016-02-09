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
/*                     Date   :  October 1997                            */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* A class for representing Stochastic Context Free Grammars             */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include "EST_Pathname.h"
#include "EST_SCFG.h"

EST_SCFG_Rule::EST_SCFG_Rule(double prob,int p, int m)
{ 
    set_rule(prob,p,m); 
}

EST_SCFG_Rule::EST_SCFG_Rule(double prob,int p, int q, int r)
{ 
    set_rule(prob,p,q,r); 
}

void EST_SCFG_Rule::set_rule(double prob,int p, int m)
{
    p_prob = prob;
    p_mother = p;
    p_daughter1 = m;
    p_type = est_scfg_unary_rule;
}

void EST_SCFG_Rule::set_rule(double prob,int p, int q, int r)
{
    p_prob = prob;
    p_mother = p;
    p_daughter1 = q;
    p_daughter2 = r;
    p_type = est_scfg_binary_rule;
}

EST_SCFG::EST_SCFG()
{
    p_prob_B=0;
    p_prob_U=0;
}

EST_SCFG::EST_SCFG(LISP rs)
{
    p_prob_B=0;
    p_prob_U=0;
    set_rules(rs);
}

EST_SCFG::~EST_SCFG(void)
{

    delete_rule_prob_cache();

}

void EST_SCFG::find_terms_nonterms(EST_StrList &nt, EST_StrList &t,LISP rs)
{
    // Cummulate the nonterminals and terminals
    LISP r;

    for (r=rs; r != NIL; r=cdr(r))
    {
	LISP p = car(cdr(car(r)));
	if (!strlist_member(nt,get_c_string(p)))
	    nt.append(get_c_string(p));
	if (siod_llength(car(r)) == 3)   // unary rule
	{
	    LISP d = car(cdr(cdr(car(r))));
	    if (!strlist_member(t,get_c_string(d)))
		t.append(get_c_string(d));
	}
	else                            // binary rules
	{
	    LISP d1 = car(cdr(cdr(car(r))));
	    LISP d2 = car(cdr(cdr(cdr(car(r)))));
	    if (!strlist_member(nt,get_c_string(d1)))
		nt.append(get_c_string(d1));
	    if (!strlist_member(nt,get_c_string(d2)))
		nt.append(get_c_string(d2));
	}
    }

}

void EST_SCFG::set_rules(LISP lrules)
{
    // Initialise rule base from Lisp form
    LISP r;
    EST_StrList nt_list, term_list;

    rules.clear();
    delete_rule_prob_cache();

    find_terms_nonterms(nt_list,term_list,lrules);
    nonterminals.init(nt_list);
    terminals.init(term_list);

    if (!consp(car(cdr(car(lrules)))))
	p_distinguished_symbol = 
	    nonterminal(get_c_string(car(cdr(car(lrules)))));
    else
	cerr << "SCFG: no distinguished symbol" << endl;

    for (r=lrules; r != NIL; r=cdr(r))
    {
	if ((siod_llength(car(r)) < 3) ||
	    (siod_llength(car(r)) > 4) ||
	    (!numberp(car(car(r)))))
	    cerr << "SCFG rule is malformed" << endl;
//	    est_error("SCFG rule is malformed\n");
	else
	{
	    EST_SCFG_Rule rule;
	    if (siod_llength(car(r)) == 3)
	    {
		int m = nonterminal(get_c_string(car(cdr(car(r)))));
		int d = terminal(get_c_string(car(cdr(cdr(car(r))))));
		rule.set_rule(get_c_float(car(car(r))),m,d);
	    }
	    else
	    {
		int p = nonterminal(get_c_string(car(cdr(car(r)))));
		int d1=nonterminal(get_c_string(car(cdr(cdr(car(r))))));
		int d2 = nonterminal(get_c_string(car(cdr(cdr(cdr(car(r)))))));
		rule.set_rule(get_c_float(car(car(r))),p,d1,d2);
	    }
	    rules.append(rule);
	}
    }

    rule_prob_cache();
}

LISP EST_SCFG::get_rules()
{
    // Return LISP form of rules
    EST_Litem *p;
    LISP r;

    for (r=NIL,p=rules.head(); p != 0; p=p->next())
    {
	if (rules(p).type() == est_scfg_unary_rule)
	    r = cons(cons(flocons(rules(p).prob()),
			  cons(rintern(nonterminal(rules(p).mother())),
			       cons(rintern(terminal(rules(p).daughter1())),NIL))),
		     r);
	else if (rules(p).type() == est_scfg_binary_rule)
	    r = cons(cons(flocons(rules(p).prob()),
		     cons(rintern(nonterminal(rules(p).mother())),
		      cons(rintern(nonterminal(rules(p).daughter1())),
		       cons(rintern(nonterminal(rules(p).daughter2())),
			    NIL)))),
		     r);
    }
    return reverse(r);
}

EST_read_status EST_SCFG::load(const EST_String &filename)
{
    LISP rs;

    rs = vload(filename,1);

    set_rules(rs);

    return format_ok;
}

EST_write_status EST_SCFG::save(const EST_String &filename)
{
    EST_Pathname outfile(filename);
    FILE *fd;
    LISP r;

    if (outfile == "-")
	fd = stdout;
    else
    {
	if ((fd=fopen(outfile,"w")) == NULL)
	{
	    cerr << "scfg_train: failed to open file \"" << outfile << 
		"\" for writing" << endl;
	    return misc_write_error;
	}
    }
    
    for (r=get_rules(); r != NIL; r=cdr(r))
	pprint_to_fd(fd,car(r));
    
    if (fd != stdout)
	fclose(fd);

    return write_ok;
}


void EST_SCFG::rule_prob_cache()
{
    // Build access cache for the probabilities of binary rules
    // This will have to made much more efficient
    int i,j;

    p_prob_B = new double**[num_nonterminals()];
    p_prob_U = new double*[num_nonterminals()];
    for (i=0; i < num_nonterminals(); i++)
    {
	p_prob_B[i] = new double*[num_nonterminals()];
	p_prob_U[i] = new double[num_terminals()];
	memset(p_prob_U[i],0,sizeof(double)*num_terminals());
	for (j=0; j < num_nonterminals(); j++)
	{
	    p_prob_B[i][j] = new double[num_nonterminals()];
	    memset(p_prob_B[i][j],0,sizeof(double)*num_nonterminals());
	}
    }

    set_rule_prob_cache();

}

void EST_SCFG::set_rule_prob_cache()
{
    EST_Litem *pp;

    for (pp=rules.head(); pp != 0; pp = pp->next())
    {
	if (rules(pp).type() == est_scfg_binary_rule)
	{
	    int p = rules(pp).mother();
	    int q = rules(pp).daughter1();
	    int r = rules(pp).daughter2();
	    p_prob_B[p][q][r] = rules(pp).prob();
	}
	else if (rules(pp).type() == est_scfg_unary_rule)
	{
	    int p = rules(pp).mother();
	    int m = rules(pp).daughter1();
	    p_prob_U[p][m] = rules(pp).prob();
	}
    }
}

void EST_SCFG::delete_rule_prob_cache()
{
    int i,j;

    if (p_prob_B == 0)
	return;

    for (i=0; i < num_nonterminals(); i++)
    {
	for (j=0; j < num_nonterminals(); j++)
	    delete [] p_prob_B[i][j];
	delete [] p_prob_B[i];
	delete [] p_prob_U[i];
    }
    delete [] p_prob_B;
    delete [] p_prob_U;

    p_prob_B = 0;
    p_prob_U = 0;
}

ostream &operator << (ostream &s, const EST_SCFG_Rule &rule)
{
  (void)rule;
  return s << "<<EST_SCFG_Rule>>";
}

Declare_TList(EST_SCFG_Rule)
#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_TList.cc"
#include "../base_class/EST_TSortable.cc"

Instantiate_TList(EST_SCFG_Rule)
#endif

