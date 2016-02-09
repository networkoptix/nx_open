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
/*  Parse a list of sentences with a given stochastic context free       */
/*  grammar                                                              */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST.h"
#include "EST_SCFG.h"
#include "EST_SCFG_Chart.h"
#include "siod.h"

static EST_String outfile = "-";

static int scfg_parse_main(int argc, char **argv);


/** @name <command>scfg_parse</command> <emphasis>Parse text using a pre-trained stochastic context free grammar</emphasis>
    @id scfg-parse-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**

This parses given text with a given stochastic context free grammar.
Note this program is not designed as an arbitrary parser for
unrestricted English.  It simply parses the input non-terminals
with the given grammar.  If you want to English (or other language)
parses consider using the festival script <command>scfg_parse</command>
which does proper tokenization and part of speech tagging, before
passing it to a SCFG.

 */

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}

int main(int argc, char **argv)
{

    scfg_parse_main(argc,argv);

    exit(0);
    return 0;
}

static int scfg_parse_main(int argc, char **argv)
{
    // Top level function generates a probabilistic grammar
    EST_Option al;
    EST_StrList files;
    EST_SCFG_Chart chart;
    LISP rules,s,parse;
    FILE *corpus,*output;
    int i;

    parse_command_line
	(argc, argv,
	 EST_String("[options]\n")+
	 "Summary: Parse a corpus with a stochastic context free grammar\n"+
	 "-grammar <ifile>  Grammar file, one rule per line.\n"+
	 "-corpus <ifile>   Corpus file, one bracketed sentence per line.\n"+
	 "-brackets          Output bracketing only.\n"+
	 "-o <ofile>        Output file for parsed sentences.\n",
		       files, al);
    
    if (al.present("-o"))
	outfile = al.val("-o");
    else
	outfile = "-";

    siod_init();

    if (al.present("-grammar"))
    {
	rules = vload(al.val("-grammar"),1);
	gc_protect(&rules);
    }
    else
    {
	cerr << "scfg_parse: no grammar specified" << endl;
	exit(1);
    }

    if (al.present("-corpus"))
    {
	if ((corpus = fopen(al.val("-corpus"),"r")) == NULL)
	{
	    cerr << "scfg_parse: can't open corpus file \"" << 
		al.val("-corpus") << "\" for reading " << endl;
	    exit(1);
	}
    }
    else
    {
	cerr << "scfg_parse: no corpus specified" << endl;
	exit(1);
    }

    if (al.present("-o"))
    {
	if ((output=fopen(al.val("-o"),"w")) == NULL)
	{
	    cerr << "scfg_parse: can't open output file \"" << 
		al.val("-o") << "\" for writing " << endl;
	    exit(1);
	}
    }
    else
	output = stdout;

    gc_protect(&s);
    gc_protect(&parse);
    for (i=0; ((s=lreadf(corpus)) != get_eof_val()); i++)
    {
	parse = scfg_parse(s,rules);
	if (al.present("-brackets"))
	{
	    LISP bparse = scfg_bracketing_only(parse);
	    if (bparse == NIL)
		bparse = s;
	    pprint_to_fd(output,bparse);
	}
	else 
	    pprint_to_fd(output,parse);
	if (i%100 == 99)
	    user_gc(NIL);
   }

    if (output != stdout)
	fclose(output);
    gc_unprotect(&s);
    gc_unprotect(&parse);
    gc_unprotect(&rules);

    return 0;
}

