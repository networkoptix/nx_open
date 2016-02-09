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
/*  Test a stochastic context free grammar with respect to a given       */
/*  corpus.                                                              */
/*                                                                       */
/*  Can test against a bracket corpus or simply parse it                 */
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

static EST_String outfile = "-";

static int scfg_test_main(int argc, char **argv);

/** @name <command>scfg_test</command> <emphasis>Test the output of a parser</emphasis>
    @id scfg-make-manual
  * @toc
 */


//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**

This program applies a stochastic context free grammar to a given
corpus and reports the parsing accuracy and cross bracketing
accuracy of the grammar with respect to the grammar.

 */

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main(int argc, char **argv)
{

    scfg_test_main(argc,argv);

    exit(0);
    return 0;
}

static int scfg_test_main(int argc, char **argv)
{
    // Top level function generates a probabilistic grammar
    EST_Option al;
    EST_StrList files;

    parse_command_line
	(argc, argv,
	 EST_String("[options]\n")+
	 "Summary: Test a stochastic context free grammar against a corpus\n"+
	 "-grammar <ifile>  Grammar file, one rule per line.\n"+
	 "-corpus <ifile>   Single Corpus file, one bracketed sentence per line.\n"+
	 "-crossbrackets    Measure cross bracket performance.\n"+
	 "-heap <int> {210000}\n"+
	 "                  Set size of Lisp heap, needed for large corpora\n"+
	 "-o <ofile>        Output file for parsed sentences.\n",
	 files, al);
    
    if (al.present("-o"))
	outfile = al.val("-o");
    else
	outfile = "-";

    siod_init(al.ival("-heap"));

    EST_SCFG_traintest grammar;

    if (al.present("-grammar"))
    {
	grammar.load(al.val("-grammar"));
    }
    else
    {
	cerr << "scfg_test: no grammar specified" << endl;
	exit(1);
    }
		
    if (al.present("-corpus"))
    {
	grammar.load_corpus(al.val("-corpus"));
    }
    else
    {
	cerr << "scfg_test: no corpus specified" << endl;
	exit(1);
    }

    // Test and summarise parsing of corpus
    if (al.present("-crossbrackets"))
	grammar.test_crossbrackets();  // parse and test brackets
    else
	grammar.test_corpus();         // only cross entropy
	
    return 0;
}
