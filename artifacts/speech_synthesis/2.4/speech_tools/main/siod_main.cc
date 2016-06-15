/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
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
/*                 Authors:  Alan W Black                                */
/*                 Date   :  February 1999                               */
/*-----------------------------------------------------------------------*/
/*  A simple wrap-around for SIOD giving a basic command line SIOD       */
/*  interpreter                                                          */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include "EST_cmd_line.h"
#include "EST_cutils.h"
#include "EST_Pathname.h"
#include "siod.h"

static void siod_lisp_vars(void);
static void siod_load_default_files(void);

/** @name <command>siod</command> <emphasis>Scheme Interpreter</emphasis>
    @id siod-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**
 
   <command>siod</command> is a command line interface to the
   <productname>Scheme In One Defun</productname> Scheme interpreter,
   as modified by CSTR for use in &theEST;. It is essentially &festival;
   without the speech synthesis code.

 */

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main(int argc, char **argv)
{
    EST_Option al;
    EST_StrList files;
    EST_Litem *p;
    int stdin_input,interactive;
    int heap_size = DEFAULT_HEAP_SIZE;

    parse_command_line
	(argc, argv, 
	 EST_String("[options] [input files]\n")+
	 "Summary: Scheme in one Defun interpreter, file arguments are loaded\n"+
         "-b            Run in batch mode (no interaction)\n"+
	 "--batch       Run in batch mode (no interaction)\n"+
	 "-i            Run in interactive mode (default)\n"+
	 "--interactive\n"+
         "              Run in interactive mode (default)\n"+
	 "--pipe        Run in pipe mode, reading commands from\n"+
	 "              stdin, but no prompt or return values\n"+
	 "              are printed (default if stdin not a tty)\n"+
	 "-heap <int> {512000}\n"+
         "             Initial size of heap\n",
	 files, al);

    if (al.present("-heap"))
	heap_size = al.ival("-heap");

    // What to do about standard input and producing prompts etc.
    if ((al.present("-i")) || (al.present("--interactive")))
    {
	interactive = TRUE;
	stdin_input = TRUE;
    }
    else if ((al.present("--pipe")))
    {
	interactive=FALSE;
	stdin_input = TRUE;
    }
    else if ((al.present("-b")) || (al.present("--batch")))
    {
	interactive=FALSE;
	stdin_input=FALSE;
    }
    else if (isatty(0))  // if stdin is a terminal assume interactive
    {   
	interactive = TRUE;
	stdin_input = TRUE;
    }
    else                     // else assume pipe mode
    {   
	interactive = FALSE;
	stdin_input = TRUE;
    }

    siod_init(heap_size);
    siod_est_init();
    /*    siod_server_init();
          siod_fringe_init(); */

    siod_prog_name = "siod";

    siod_lisp_vars();

     if (interactive)
	siod_load_default_files();

    for (p=files.head(); p != 0; p=p->next())
    {
	if (files(p) == "-")
	    continue;
	else if (files(p).matches(make_regex("^(.*")))
	{
	    LISP l;
	    l = read_from_string(files(p));
	    leval(l,NIL);
	}
	else
	    vload(files(p),0);

    }

    if (stdin_input)
    {
      siod_print_welcome(EST_String::cat("Modified for ", est_name, " v", est_tools_version));
	siod_repl(interactive);        // expect input from stdin
    }

    return 0;
}

static void siod_load_default_files(void)
{
    // Load in default files, init.scm. Users ~/.festivalrc 
    // (or whatever you wish to call it) is loaded by init.scm

    EST_Pathname initfile;

    // Load library init first
    initfile = EST_Pathname(est_libdir).as_directory();
    initfile += "siod";
    initfile += "init.scm";

    if (access((const char *)initfile,R_OK) == 0)
	vload(initfile,FALSE);
    else
	cerr << "Initialization file " << initfile << " not found" << endl;
}

static void siod_lisp_vars(void)
{
    // set up specific lisp variables 
    int major=0,minor=0,subminor=0;

    EST_Pathname lib;

    lib = EST_Pathname(est_libdir).as_directory();
    lib += "siod";
    
    siod_set_lval("libdir",strintern(lib));

    if (!strcmp(est_ostype,""))
      siod_set_lval("*ostype*",rintern(est_ostype));
    siod_set_lval("est_version",
		  strcons(strlen(est_tools_version),est_tools_version));

    EST_String bits[4];
    EST_Regex sep = "[^0-9]+";
    int nbits = split(est_tools_version, bits, 4, sep);

    if (nbits>0)
      major = bits[0].Int();
    if (nbits>1)
      minor = bits[1].Int();
    if (nbits>2)
      subminor = bits[2].Int();

    siod_set_lval("est_version_number",
		  cons(flocons(major),
		       cons(flocons(minor),
			    cons(flocons(subminor),NIL))));

    EST_Pathname etcdircommon = est_libdir;
    etcdircommon += "etc";

    EST_Pathname etcdir = etcdircommon;
    etcdir += est_ostype;
    
    //  Modify my PATH to include these directories
    siod_set_lval("etc-path",cons(rintern(etcdir),
				  cons(rintern(etcdircommon),NIL)));

    EST_String path = getenv("PATH");

    path += ":" + EST_String(etcdir) + ":" +  EST_String(etcdircommon);

    putenv(wstrdup("PATH=" + path));
    
    siod_set_lval("*modules*",NIL);

    return;
}

