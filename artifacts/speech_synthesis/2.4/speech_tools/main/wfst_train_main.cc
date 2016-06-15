/*************************************************************************/
/*                                                                       */
/*                  Language Technologies Institute                      */
/*                     Carnegie Mellon University                        */
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
/*  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK         */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE      */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                     Author :  Alan W Black                            */
/*                     Date   :  October 1999                            */
/*-----------------------------------------------------------------------*/
/*  A training method for splitting states in a WFST from data           */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST.h"
#include "EST_simplestats.h"
#include "EST_WFST.h"

LISP load_string_data(EST_WFST &wfst,EST_String &filename);
void wfst_train(EST_WFST &wfst, LISP data);

static int wfst_train_main(int argc, char **argv);

/** @name <command>wfst_train</command> <emphasis>Train a weighted finite-state transducer</emphasis>
    @id wfst-train-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**
This takes an existing WFST and data and splits states in an entropy
reduce way to produced a new WFST that better models the given data.

 */

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main(int argc, char **argv)
{

    wfst_train_main(argc,argv);

    exit(0);
    return 0;
}

static int wfst_train_main(int argc, char **argv)
{
    // Train a WFST from data building new states
    EST_Option al;
    EST_StrList files;
    EST_String wfstfile;
    FILE *ofd;

    parse_command_line
	(argc, argv,
	 EST_String("[WFSTFILE] [input file0] ... [-o output file]\n")+
	 "Summary: Train a WFST on data\n"+
	 "-wfst <ifile>    The WFST to start from\n"+
	 "-data <ifile>    Sentences in the language recognised by WFST\n"+
	 "-o <ofile>       Output file for trained WFST\n"+
	 "-heap <int> {210000}\n"+
	 "                    Set size of Lisp heap, needed for large rulesets\n",
	 files, al);
    
    if (al.present("-o"))
    {
	if ((ofd=fopen(al.val("-o"),"w")) == NULL)
	    EST_error("can't open output file for writing \"%s\"",
		      (const char *)al.val("-o"));
    }
    else
	ofd = stdout;

    if (al.present("-wfst"))
	wfstfile = al.val("-wfst");
    else
	EST_error("no WFST specified");
    
    siod_init(al.ival("-heap"));
    siod_est_init();

    EST_WFST wfst;
    LISP data;

    if (wfst.load(wfstfile) != format_ok)
	EST_error("failed to read WFST from \"%s\"",
			  (const char *)wfstfile);

    data = load_string_data(wfst,al.val("-data"));

    wfst_train(wfst,data);
    
    if (wfst.save(al.val("-o")) != write_ok)
	EST_error("failed to write trained WFST to \"%s\"",
		      (const char *)al.val("-o"));

    return 0;
    
}

