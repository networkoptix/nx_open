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
/*  Train a stochastic context free grammar with respect to a given      */
/*  corpus.                                                              */
/*                                                                       */
/*  Only the inside/outside algorithm (with bracketing) is supported     */
/*                                                                       */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST_cmd_line.h"
#include "EST_SCFG.h"
#include "siod.h"

static EST_String outfile = "-";


static int scfg_train_main(int argc, char **argv);

/** @name <command>scfg_train</command> <emphasis>Train the parameters of a stochastic context free grammar</emphasis>
    @id scfg-make-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**

scfg_train takes a stochastic context free grammar (SCFG) and trains
the probabilities with respect to a given bracket corpus using the
inside-outside algorithm.   This is basically an implementation
of Pereira and Schabes 1992.

Note using this program properly may require months of CPU time.

 */

//@}

/**@name OPTIONS
  */
//@{


//@options

//@}


int main(int argc, char **argv)
{

    scfg_train_main(argc,argv);

    exit(0);
    return 0;
}

static int scfg_train_main(int argc, char **argv)
{
    // Top level function generates a probabilistic grammar
    EST_Option al;
    EST_StrList files;
    int spread;

    parse_command_line
	(argc, argv,
	 EST_String("[options\n")+
	 "Summary: Train a stochastic context free grammar from a (bracketed) corpus\n"+
	 "-grammar <ifile>  Grammar file, one rule per line.\n"+
	 "-corpus <ifile>   Corpus file, one bracketed sentence per line.\n"+
	 "-method <string> {inout}\n"+
	 "                  Method for training: inout.\n"+
	 "-passes <int> {50}\n"+
	 "                  Number of training passes.\n"+
	 "-startpass <int> {0}\n"+
	 "                  Starting at pass N.\n"+
	 "-spread <int>     Spread training data over N passes.\n"+
	 "-checkpoint <int> Save grammar every N passes\n"+
	 "-heap <int> {210000}\n"+
	 "                  Set size of Lisp heap, needed for large corpora\n"+
	 "-o <ofile>        Output file for trained grammar.\n",
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
	cerr << "scfg_train: no grammar specified" << endl;
	exit(1);
    }
		
    if (al.present("-corpus"))
    {
	grammar.load_corpus(al.val("-corpus"));
    }
    else
    {
	cerr << "scfg_train: no corpus specified" << endl;
	exit(1);
    }

    if (al.present("-spread"))
	spread = al.ival("-spread");
    else
	spread = 0;

    if (al.val("-method") == "inout")
    {
	int checkpoint = -1;
	if (al.present("-checkpoint"))
	    checkpoint = al.ival("-checkpoint");

	grammar.train_inout(al.ival("-passes"),
			    al.ival("-startpass"),
			    checkpoint,spread,outfile);
    }
    else
    {
	cerr << "scfg_train: unknown training method \"" << 
	    al.val("-method") << "\"" << endl;
	exit(1);
    }

    if (grammar.save(outfile) != write_ok)
    {
	cerr << "scfg_train: failed to write grammar to \"" << 
	    outfile << "\"" << endl;
	exit(1);
    }
	
    return 0;
}
