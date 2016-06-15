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
/*                     Date   :  October 1997                            */
/*-----------------------------------------------------------------------*/
/*  Build a stochastic context feee grammar with N non-terminals and     */
/*  M terminals specific as lists or numbers                             */
/*  Probabilities are either even or random on rules and specified as    */
/*  probs or -log prob                                                   */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST.h"
#include "EST_SCFG.h"
#include "siod.h"

EST_String outfile = "-";
EST_String domain = "nlogp";
EST_String values = "equal";

static int scfg_make_main(int argc, char **argv);

static void load_symbols(EST_StrList &syms,const EST_String &filename);
static void make_symbols(EST_StrList &syms,int n,const EST_String &prefix);
static LISP assign_probs(LISP rules, const EST_String &domain, 
			 const EST_String &values);
static LISP make_all_rules(const EST_StrList &NonTerminals,
			   const EST_StrList &Terminals);
static void generate_probs(double *probs,int num);

/** @name <command>scfg_make</command> <emphasis>Make the rules for a stochastic context free grammar</emphasis>
    @id scfg-make-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**

Builds a stochastic context free grammar from a vocabulary of non-terminal
and terminal symbols.  An exhaustive set of all possible binary rules
are generated with random (or equal) probabilities (or negative log
probabilities).  This program is designed for making grammars that
can be trained using scfg_train.

 */

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main(int argc, char **argv)
{

    scfg_make_main(argc,argv);

    exit(0);
    return 0;
}

static int scfg_make_main(int argc, char **argv)
{
    // Top level function generates a probabilistic grammar
    EST_Option al;
    EST_StrList files;
    EST_StrList NonTerminals, Terminals;
    LISP rules,r;
    FILE *fd;

    parse_command_line
	(argc, argv,
       EST_String("[options]\n")+
       "Summary: Build a stochastic context free grammar\n"+
       "-nonterms <string>  Number of nonterminals or file containing them\n"+
       "-terms <string>     Number of terminals or file containing them\n"+
       "-domain <string> {nlogp}\n"+
       "                    Values to be nlogp (negative log probabilities)\n"+
       "                    or prob (probabilities)\n"+
       "-values <string> {equal}\n"+
       "                    General initial scores on rules as equal or\n"
       "                    random\n"+
       "-heap <int> {500000}\n"+
       "              Set size of Lisp heap, only needed for large grammars\n"+
       "-o <ofile>          File to save grammar (default stdout)\n",
		       files, al);
    
    if (al.present("-o"))
	outfile = al.val("-o");
    else
	outfile = "-";

    if (al.present("-domain"))
    {
	if (al.val("-domain") == "nlogp")
	    domain = "nlogp";
	else if (al.val("-domain") == "prob")
	    domain = "prob";
	else
	{
	    cerr << "scfg_make: domain must be nlogp or prob" << endl;
	    exit(1);
	}
    }

    if (al.present("-values"))
    {
	if (al.val("-values") == "equal")
	    values = "equal";
	else if (al.val("-values") == "random")
	    values = "random";
	else
	{
	    cerr << "scfg_make: values must be equal or random" << endl;
	    exit(1);
	}
    }

    if (al.present("-nonterms"))
    {
	if (al.val("-nonterms").matches(RXint))
	    make_symbols(NonTerminals,al.ival("-nonterms"),"NT");
	else 
	    load_symbols(NonTerminals,al.val("-nonterms"));
    }
    else
    {
	cerr << "scfg_make: no nonterminals specified" << endl;
	exit(1);
    }
		
    if (al.present("-terms"))
    {
	if (al.val("-terms").matches(RXint))
	    make_symbols(Terminals,al.ival("-terms"),"T");
	else 
	    load_symbols(Terminals,al.val("-terms"));
    }
    else
    {
	cerr << "scfg_make: no terminals specified" << endl;
	exit(1);
    }

    siod_init(al.ival("-heap"));

    rules = make_all_rules(NonTerminals,Terminals);
    rules = assign_probs(rules,domain,values);

    if (outfile == "-")
	fd = stdout;
    else
    {
	if ((fd=fopen(outfile,"w")) == NULL)
	{
	    cerr << "scfg_make: failed to open file \"" << outfile << 
		"\" for writing" << endl;
	    exit(1);
	}
    }
    
    for (r=rules; r != NIL; r=cdr(r))
	pprint_to_fd(fd,car(r));
    
    if (fd != stdout)
	fclose(fd);

		
    return 0;
}

static LISP make_all_rules(const EST_StrList &NonTerminals,
			   const EST_StrList &Terminals)
{
    // Build all possibly rules (CNF)
    //  NT -> NT NT and NT -> T
    EST_Litem *p,*q,*r;
    LISP rules = NIL;
	
    for (p=NonTerminals.head(); p != 0; p=p->next())
    {
	int num_rules_nt = (NonTerminals.length()*NonTerminals.length())+
	    Terminals.length();
	double *probs = new double[num_rules_nt];
	generate_probs(probs,num_rules_nt);
	int i=0;
	for (q=NonTerminals.head(); q != 0; q=q->next())
	    for (r=NonTerminals.head(); r != 0; r=r->next(),i++)
		rules = cons(cons(flocons(probs[i]),
				  cons(rintern(NonTerminals(p)),
				  cons(rintern(NonTerminals(q)),
				  cons(rintern(NonTerminals(r)),NIL)))),
			     rules);
	for (q=Terminals.head(); q != 0; q=q->next(),i++)
	    rules = cons(cons(flocons(probs[i]),
			      cons(rintern(NonTerminals(p)),
				   cons(rintern(Terminals(q)),NIL))),
			 rules);
	delete [] probs;
    }

    return reverse(rules);
}

static void generate_probs(double *probs,int num)
{
    // Generate probabilities 
    int i;

    if (values == "equal")
    {
	double defp = 1.0/(float)num;
	for (i=0; i < num; i++)
	    probs[i] = defp;
    }
    else if (values == "random")
    {
	// This isn't random but is somewhat arbitrary
	double sum = 0;
	for (i=0; i < num; i++)
	{
	    probs[i] = (double)abs(rand())/(double)0x7fff;
	    sum += probs[i];
	}
	for (i=0; i < num; i++)
	{
	    probs[i] /= sum;
	}
    }
    else
    {
	cerr << "scfg_make: unknown value for probability distribution"
	    << endl;
	exit(1);
    }
}

static LISP assign_probs(LISP rules, const EST_String &domain, 
			 const EST_String &values)
{
    // Modify probs (don't know how to do random probs yet)
    LISP r;
    (void)values;

    if (domain == "nlogp")
	for (r=rules; r != NIL; r = cdr(r))
        {
	    if (get_c_float(car(car(r))) == 0)
		CAR(car(r)) = flocons(40);
	    else
		CAR(car(r)) = flocons(-log(get_c_float(car(car(r)))));
        }

    return rules;
}

static void make_symbols(EST_StrList &syms,int n,const EST_String &prefix)
{
    //  Generate n symbols with given prefix
    int i;
    int magnitude,t;

    for (magnitude=0,t=n; t > 0; t=t/10)
	magnitude++;

    char *name = walloc(char,prefix.length()+magnitude+1);
    char *skel = walloc(char,prefix.length()+5);
    sprintf(skel,"%s%%%02dd",(const char *)prefix,magnitude);

    for (i=0; i < n; i++)
    {
	sprintf(name,skel,i);
	syms.append(name);
    }

    wfree(name);
    wfree(skel);

}	


static void load_symbols(EST_StrList &syms,const EST_String &filename)
{
    //  Load symbol list for file

    load_StrList(filename,syms);

}	
