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
/*                     Date   :  December 1997                           */
/*-----------------------------------------------------------------------*/
/*  Run a WFST on some data, either as a recognizer or as a transducer   */
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

static int wfst_run_main(int argc, char **argv);

/** @name <command>wfst_run</command> <emphasis>Run a weighted finite-state transducer</emphasis>
    @id wfst-run-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**
This program runs a WFST on some given data.  It works in either
recognize mode where both inputs and output are specified, but also
in transduction mode where an input is transduced to the output.

 */

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main(int argc, char **argv)
{

    wfst_run_main(argc,argv);

    exit(0);
    return 0;
}

static int wfst_run_main(int argc, char **argv)
{
    // recognize/transduce
    EST_Option al;
    EST_StrList files;
    EST_Litem *f;
    EST_String wfstfile;
    FILE *ofd;
    int r;
    EST_SuffStats R;
    float sumlogp=0,isumlogp;
    float count=0,icount;

    parse_command_line
	(argc, argv,
	 EST_String("[WFSTFILE] [input file0] ... [-o output file]\n")+
	 "Summary: Recognize/transduce using a WFST on data\n"+
	 "-wfst <ifile>    The WFST to use\n"+
	 "-transduce       Transduce input to output (default)\n"+
	 "-recog           Recognize input consists of pairs\n"+
	 "-cumulate_into <ofile>\n"+
	 "                 Cumulate transitions to give new weights\n"+
	 "                 save new WFST into ofile\n"+
	 "-itype <string>  char or token\n"+
	 "-quiet           No extraneous messages\n"+
	 "-perplexity      Calculate perplexity on given data set\n"+
	 "-heap <int> {210000}\n"+
	 "                    Set size of Lisp heap, needed for large wfsts\n"+
	 "-o <ofile>       Output file for transduced forms\n",
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

    EST_WFST wfst;
    EST_TokenStream ts;

    if (wfst.load(wfstfile) != format_ok)
	EST_error("failed to read WFST from \"%s\"",
			  (const char *)wfstfile);

    if (al.present("-cumulate_into"))
	wfst.start_cumulate();

    for (f=files.head(); f != 0; f=f->next())
    {
	if (files(f) == "-")
	    ts.open(stdin,FALSE);
	else
	    if (ts.open(files(f)) != 0)
		EST_error("failed to read WFST data file from \"%s\"",
			  (const char *)files(f));
	    
	// Not the best way to input things but will do the the present
	while(!ts.eof())
	{
	    EST_StrList ostrs,istrs;
	    do
		istrs.append(ts.get());
	    while((!ts.eof()) && (!ts.eoln()));

	    if (al.present("-recog"))
	    {
		if (al.present("-perplexity"))
		{
		    r = recognize_for_perplexity(wfst,istrs,
						 al.present("-quiet"),
						 icount,
						 isumlogp);
		    if (r)
		    {
			count += icount;
			sumlogp += isumlogp;
		    }
		}
		else
		    r = recognize(wfst,istrs,al.present("-quiet"));
	    }
	    else
	    {
		r = transduce(wfst,istrs,ostrs);
		if (r)
		{
		    cout << ostrs;
		    cout << endl;
		}
	    }
	    R += r;

	    if (!al.present("-quiet"))
	    {
		if (r)
		    cout << "OK." << endl;
		else
		    cout << "failed." << endl;
	    }
	}
	ts.close();
    }

    if (al.present("-cumulate_into"))
    {
	wfst.stop_cumulate();
	if (wfst.save(al.val("-cumulate_into")) != write_ok)
	    EST_error("failed to write cumulated WFST to \"%s\"",
		      (const char *)al.val("-cumulate_into"));
    }
    
    printf("total %d OK %f%% failed %f%%\n",
	   (int)R.samples(),R.mean()*100,(1-R.mean())*100);
    if (al.present("-perplexity"))
    {
      printf("perplexity is %f\n", pow(float(2.0),float(-1.0 * (sumlogp/count))));
    }

    if (ofd != stdout)
	fclose(ofd);

    if (R.mean() == 1)     // true is *all* files were recognized
	return 0;
    else
	return -1;
}

