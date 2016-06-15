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
/*                     Date   :  November 1997                           */
/*-----------------------------------------------------------------------*/
/*  Build a WFST from some base:                                         */
/*  1 a set of context dependent rewrite rules using the                 */
/*    the algorithms from "An Efficient Compiler for Weighted Rewrite    */
/*    Rules", by Mehryar Mohri and Richard Sproat ACL 1996               */
/*    and information from the techniques in Rithie el al. 1992          */
/*  2 A regular grammar (but can be written as a CFG as long as it       */
/*    contains no centre embedding                                       */
/*  3 A regular expression                                               */
/*  4 lts rules (but that doesn't work yet)                              */
/*                                                                       */
/*  or apply some operator on existing wfst(s): compose, concatenate,    */
/*  difference, union,                                                   */
/*                                                                       */
/*  Also allow determinizing and minimization as required                */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST.h"
#include "EST_WFST.h"

static int wfst_build_main(int argc, char **argv);



/** @name <command>wfst_build</command> <emphasis>Build a weighted finite-state transducer</emphasis>
    @id wfst-build-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**

Build and.or process weighted finite state transducers (WFSTs) form
various input formats.  This program accepts descriptions
in the following formats and converts them to WFSTs
<itemizedlist>
<listitem><para>regular expressions</para></listitem>
<listitem><para>regular grammars</para></listitem>
<listitem><para>Koskenniemi/Kay/Kaplan context restriction rules</para></listitem>
</itemizedlist>
In addition various operations can be performed on two WFSTs
<itemizedlist>
<listitem><para>compose: form new WFST feeding output of first WFSTs into
second WFSTs.</para></listitem>
<listitem><para>union: form new WFST accepting the language both WFSTs
</para></listitem>
<listitem><para>intersect: form new WFST accepting only the language common
to both WFSTs
</para></listitem>
<listitem><para>concat: form new WFST accepting the language from the
concatenation of all strings in the first WFST to all strings in the
second.
</para></listitem>
</itemizedlist>
The newly formed WFSTs can be optionally determinized and minimzed.

The option asis allows a single WFSTs to be loaded and determinized
and/or minimized

 */

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}
int main(int argc, char **argv)
{

    wfst_build_main(argc,argv);

    exit(0);
    return 0;
}

static int wfst_build_main(int argc, char **argv)
{
    // Top level function generates a WFST from rules
    EST_Option al;
    EST_StrList files;
    EST_String outfile;

    parse_command_line
	(argc, argv,
	 EST_String("[option] [rulefile0] [rulefile1] ...\n")+
	 "Summary: Build a weighted finite state transducer from rules/wfsts\n"+
	 "-type <string> {kk} Input rule type: kk, lts, rg, tl, compose, regex\n"+
	 "                    union, intersect, concat, asis\n"+
	 "-determinize        Determinize WFST before saving it\n"+
	 "-detmin             Determinize and minimize WFST before saving it\n"+
	 "-o <ofile>          Output file for saved WFST (default stdout)\n"+
	 "-otype <string> {ascii}\n"+
         "                    Output type, ascii or binary\n"+
	 "-heap <int> {210000}\n"+
	 "                    Set size of Lisp heap, needed for large rulesets\n"+
	 "-q                  Quiet mode, no summary generated\n",
		       files, al);
    
    if (al.present("-o"))
	outfile = al.val("-o");
    else
	outfile = "-";

    siod_init(al.ival("-heap"));

    LISP ruleset;
    LISP inalpha, outalpha;
    EST_WFST *wfst = new EST_WFST;
    gc_protect(&ruleset);

    if (al.val("-type") == "kk")
    {
	ruleset = car(vload(files(files.head()),1));
	kkcompile(ruleset,*wfst);
    }
    else if (al.val("-type") == "lts")
    {
	ruleset = car(vload(files(files.head()),1));
	ltscompile(ruleset,*wfst);
    }
    else if (al.val("-type") == "rg")
    {
	ruleset = car(vload(files(files.head()),1));
	rgcompile(ruleset,*wfst);
    }
    else if (al.val("-type") == "tl")
    {
	ruleset = car(vload(files(files.head()),1));
	tlcompile(ruleset,*wfst);
    }
    else if (al.val("-type") == "asis")
    {
	if (wfst->load(files.nth(0)) != format_ok) exit(-1);
    }
    else if (al.val("-type") == "compose")
    {
	EST_WFST a,b;
	
	if (files.length() != 2)
	    EST_error("compose requires two WFSTs to combine");

	if (a.load(files.nth(0)) != format_ok) exit(-1);
	if (b.load(files.nth(1)) != format_ok) exit(-1);
	
	wfst->compose(a,b);
    }
    else if (al.val("-type") == "union")
    {
	EST_WFST a,b;

	if (files.length() != 2)
	    EST_error("union requires two WFSTs to combine");
	
	if (a.load(files.nth(0)) != format_ok) exit(-1);
	if (b.load(files.nth(1)) != format_ok) exit(-1);
	
	wfst->uunion(a,b);
    }
    else if (al.val("-type") == "intersect")
    {
	EST_WFST a,b;
	
	if (files.length() != 2)
	    EST_error("intersect requires two WFSTs to combine");
	if (a.load(files.nth(0)) != format_ok) exit(-1);
	if (b.load(files.nth(1)) != format_ok) exit(-1);
	
	wfst->intersection(a,b);
    }
    else if (al.val("-type") == "concat")
    {
	EST_WFST a,b;
	
	if (files.length() != 2)
	    EST_error("concat requires two WFSTs to combine");
	if (a.load(files.nth(0)) != format_ok) exit(-1);
	if (b.load(files.nth(1)) != format_ok) exit(-1);
	
 	wfst->concat(a,b);
    }
    else if (al.val("-type") == "difference")
    {
	EST_WFST a,b;
	
	if (files.length() != 2)
	    EST_error("difference requires two WFSTs to combine");
	if (a.load(files.nth(0)) != format_ok) exit(-1);
	if (b.load(files.nth(1)) != format_ok) exit(-1);
	
	wfst->difference(a,b);
    }
    else if (al.val("-type") == "regex")
    {
	ruleset = car(vload(files(files.head()),1));
	inalpha = siod_nth(0,ruleset);
	outalpha = siod_nth(1,ruleset);
	wfst->build_from_regex(inalpha,outalpha,car(cdr(cdr(ruleset))));
    }
    else
    {
	cerr << "wfst_build: unknown rule type \"" << al.val("-type") 
	    << "\"" << endl;
	exit(-1);
    }	

    if (al.present("-determinize"))
    {
	EST_WFST *dwfst = new EST_WFST;
	dwfst->determinize(*wfst);
	if (!al.present("-q"))
	{
	    cout << "wfst_build summary: " << endl;
	    cout << "   non-deterministic wfst: " << 
		wfst->summary() << endl;
	    cout << "       deterministic wfst: " << 
		dwfst->summary() << endl;
	}
	delete wfst;
	wfst = dwfst;
    }
    else if (al.present("-detmin"))
    {
	if (!al.present("-q"))
	{
	    cout << "wfst_build summary: " << endl;
	    cout << "   non-deterministic wfst: " << 
		wfst->summary() << endl;
	}
	EST_WFST *dwfst = new EST_WFST;
	dwfst->determinize(*wfst);
	delete wfst;
	if (!al.present("-q"))
	    cout << "       deterministic wfst: " << 
		dwfst->summary() << endl;
	EST_WFST *mwfst = new EST_WFST;
	mwfst->minimize(*dwfst);
	if (!al.present("-q"))
	    cout << "           minimized wfst: " << 
		mwfst->summary() << endl;
	delete dwfst;
	wfst = mwfst;
    }
    else
    {
	if (!al.present("-q"))
	    cout << "wfst_build: " << wfst->summary() << endl;
    }

    wfst->save(outfile,al.val("-otype"));
    delete wfst;
    gc_unprotect(&ruleset);

    return 0;
}

