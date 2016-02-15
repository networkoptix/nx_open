/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                    Copyright (c) 1994,1995,1996                       */
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
/*                       Author :  Paul Taylor                           */
/*                       Date   :  October 1994                          */
/*-----------------------------------------------------------------------*/
/*                      Command Line Utilities                           */
/*                                                                       */
/*  awb merged the help usage and argument definitions April 97          */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include "EST_unix.h"
#include "EST_String.h"
#include "EST_io_aux.h"
#include "EST_Token.h"
#include "EST_cutils.h"
#include "EST_TList.h"
#include "EST_string_aux.h"
#include "EST_cmd_line.h"
#include "EST_Pathname.h"
#include "EST_Features.h"

// This is reset by the command line options functions to argv[0]
EST_String est_progname = "ESTtools";

static int valid_option(const EST_Option &option,const char *arg,
			EST_String &sarg);
static void standard_options(int argc, char **argv, const EST_String &usage);
static void arg_error(const EST_String &progname, const EST_String &message);
static void parse_usage(const EST_String &progname, const EST_String &usage, 
			EST_Option &options, EST_Option &al);
static void output_man_options(const EST_String &usage);
static void output_sgml_options(const EST_String &usage);
static void output_sgml_synopsis(char **argv, const EST_String &usage);

int init_lib_ops(EST_Option &al, EST_Option &op)
{
    char *envname;
    
    // read environment variable operations file if specified
    if ((al.val("-N", 0) != "true") && 
	((envname = getenv("IA_OP_FILE")) != 0))
	if (op.load(getenv("IA_OP_FILE")) != read_ok)
	    exit (1);
    
    // read command line operations file if specified
    if (al.val("-c", 0) != "") 
	if (op.load(al.val("-c")) != read_ok)
	    exit (1);
    
    // override operations with command line options
    override_lib_ops(op, al); 

    if (al.val("-ops", 0) == "true") // print options if required
	cout << op;
    
    return 0;
}

//  An attempt to integrate help, usage and argument definitions in 
//  one string (seems to work)
//  Still to do: 
// *   adding arbitrary "-" at end of files (do we need that?)
//     dealing with environment var specification of extra options
//     override options function (maybe no longer needed)
//     list of named values for argument
//     Way to identify mandatory arguments
int parse_command_line(int argc, 
		       char *argv[],
		       const EST_String &usage,
		       EST_StrList &files,
		       EST_Option &al, int make_stdio)
{
    // Parse the command line arguments returning them in a normalised
    // form in al, and files in files.
    int i;
    EST_Option options;
    EST_String arg;
    (void)make_stdio;

    // help, version, man_options are always supported
    standard_options(argc,argv,usage); 

    // Find valid options, arguments and defaults
    // sets defaults in al
    est_progname = argv[0];
    parse_usage(argv[0],usage,options,al);

    for (i=1; i < argc; i++)
    {
	if (!EST_String(argv[i]).contains("-",0))  // its a filename
	    files.append(argv[i]);
	else if (streq(argv[i],"-"))   // single "-" denotes stdin/out
	    files.append("-");
	else if (!valid_option(options,argv[i],arg))   
	{
	    arg_error(argv[0],
		      EST_String(": Unknown option \"")+argv[i]+"\"\n");
	}
	else                                       // valid option, check args
	{
	    if (options.val(arg) == "true")  // no argument required
		al.add_item(arg, "true");
	    else if (options.val(arg) == "<int>")
	    {
		if (i+1 == argc)
		    arg_error(argv[0],
			      EST_String(": missing int argument for \"")+
			      arg+"\"\n");
		i++;
		if (!EST_String(argv[i]).matches(RXint))
		    arg_error(argv[0],
			      EST_String(": argument for \"")+
			      arg+"\" not an int\n");
		al.add_item(arg,argv[i]);
	    }
	    else if ((options.val(arg) == "<float>") ||
		     (options.val(arg) == "<double>"))
	    {
		if (i+1 == argc)
		    arg_error(argv[0],
			      EST_String(": missing float argument for \"")+
			      arg+"\"\n");
		i++;
		if (!EST_String(argv[i]).matches(RXdouble))
		    arg_error(argv[0],
			      EST_String(": argument for \"")+
			      arg+"\" not a float\n");
		al.add_item(arg,argv[i]);
	    }
	    else if (options.val(arg) == "<string>")
	    {
		if (i+1 == argc)
		    arg_error(argv[0],
			      EST_String(": missing string argument for \"")+
			      arg+"\"\n");
		i++;
		al.add_item(arg,argv[i]);
	    }
	    else if (options.val(arg) == "<ofile>")
	    {
		if (i+1 == argc)
		    arg_error(argv[0],
			      EST_String(": missing ifile argument for \"")+
			      arg+"\"\n");
		i++;
		if (writable_file(argv[i]) == TRUE)
		    al.add_item(arg,argv[i]);
		else 
		    arg_error(argv[0],
			      EST_String(": output file not accessible \"")+
			      argv[i]+"\"\n");
	    }
	    else if (options.val(arg) == "<ifile>")
	    {
		if (i+1 == argc)
		    arg_error(argv[0],
			      EST_String(": missing ifile argument for \"")+
			      arg+"\"\n");
		i++;
		if (readable_file(argv[i]) == TRUE)
		    al.add_item(arg,argv[i]);
		else
		    arg_error(argv[0],
			      EST_String(": input file not accessible \"")+
			      argv[i]+"\"\n");

	    }
	    else if (options.val(arg) == "<star>")
	    {
		al.add_item(arg,EST_String(argv[i]).after(arg));
	    }
	    // else string list 
	    else
	    {
		arg_error(argv[0],
			  EST_String(": unknown argument type \"")+
			  options.val(argv[i])+"\" (misparsed usage string)\n");
	    }
	}
    }

    if (files.length() == 0)
	files.append("-");

    return 0;
}

static int valid_option(const EST_Option &options,const char *arg,
			EST_String &sarg)
{
    // Checks to see arg is declared as an option.
    // This would be trivial were it not for options containing *
    // The actual arg name is put in sarg
    EST_Litem *p;
    
    for (p = options.list.head(); p != 0; p = p->next())
    {
	if (options.key(p) == arg)
	{
	    sarg = arg;
	    return TRUE;
	}
	else if ((options.val(p) == "<star>") &&
		 (EST_String(arg).contains(options.key(p), 0)))
	{
	    sarg = options.key(p);
	    return TRUE;
	}
    }
    
    return FALSE;
}

static void parse_usage(const EST_String &progname, const EST_String &usage, 
			EST_Option &options, EST_Option &al)
{
    // Extract option definitions from usage and put them in options
    // If defaults are specified add them al
    EST_TokenStream ts;
    EST_Token t;
    
    ts.open_string(usage);
    ts.set_SingleCharSymbols("{}[]|");
    ts.set_PunctuationSymbols("");
    ts.set_PrePunctuationSymbols("");
    
    while (!ts.eof())
    {
	t = ts.get();
	if ((t.string().contains("-",0)) &&
	    (t.whitespace().contains("\n")))
	{			// An argument
	    if ((ts.peek().string() == "<string>") ||
		(ts.peek().string() == "<float>") ||
		(ts.peek().string() == "<double>") ||
		(ts.peek().string() == "<ifile>") ||
		(ts.peek().string() == "<ofile>") ||
		(ts.peek().string() == "<int>"))
	    {
		options.add_item(t.string(),ts.get().string());	
		if (ts.peek().string() == "{") // a default is given
		{
		    ts.get();
		    al.add_item(t.string(),ts.get().string());
		    if (ts.get() != "}")
			arg_error(progname,
				  EST_String(": malformed default value for \"")+
				  t.string()+"\" (missing closing brace)\n");
		}
	    }
	    else if (t.string().contains("*"))
		options.add_item(t.string().before("*"),"<star>");
	    // else check for explicit list of names 
	    else
		options.add_item(t.string(),"true"); // simple argument
	}
    }
}

static void arg_error(const EST_String &progname, const EST_String &message)
{
    // Output message  and pointer to more help then exit
    cerr << progname << message;
    cerr << "Type -h for help on options.\n";
    exit(-1);
}

static void standard_options(int argc, char **argv, const EST_String &usage)
{
    // A number of options are always supported 
    int i;
    
    for (i=1; i < argc; i++)
    {
	if (streq(argv[i],"-man_options"))
	{
	    output_man_options(usage);
	    exit(0);
	}
	if (streq(argv[i],"-sgml_options"))
	{
	    output_sgml_options(usage);
	    exit(0);
	}
	if (streq(argv[i],"-sgml_synopsis"))
	{
	    output_sgml_synopsis(argv, usage);
	    exit(0);
	}
	if ((streq(argv[i],"-h")) ||
	    (streq(argv[i],"-help")) ||
	    (streq(argv[i],"-?")) ||
	    (streq(argv[i],"--help")))
	{
	    EST_Pathname full(argv[0]);
	    cout << "Usage: " << full.filename() << " " << usage << endl;
	    exit(0);
	}
	if (((streq(argv[i],"-version")) ||
	     (streq(argv[i],"--version")))&&
	    (!usage.contains("\n-v")))
	{
	  cout << argv[0] << ": " << est_name << " v" << est_tools_version << endl;
	    exit(0);
	}
    }
    
    return;
}

static void output_man_options(const EST_String &usage)
{
    EST_TokenStream ts;
    EST_Token t;
    int in_options = FALSE;
    
    ts.open_string(usage);
    ts.set_SingleCharSymbols("{}[]|");
    ts.set_PunctuationSymbols("");
    ts.set_PrePunctuationSymbols("");
    
    while (!ts.eof())
    {
	t = ts.get();
	if ((t.string().contains("-",0)) &&
	    (t.whitespace().contains("\n")))
	{			// An argument
	    fprintf(stdout,"\n.TP 8\n.BI \"%s \" ",(const char *)t.string());
	    if ((ts.peek().string() == "<string>") ||
		(ts.peek().string() == "<float>") ||
		(ts.peek().string() == "<double>") ||
		(ts.peek().string() == "<int>"))
		fprintf(stdout,"%s",(const char *)ts.get().string());
	    if ((ts.peek().string() == "{"))
	    {			// a default value
		ts.get();
		fprintf(stdout," \" {%s}\"",(const char *)ts.get().string());
		ts.get();
	    }
	    if (!ts.peek().whitespace().contains("\n"))
		fprintf(stdout,"\n");
	    in_options = TRUE;
	}
	else if (in_options)
	{
	    if (t.whitespace().contains("\n"))
		fprintf(stdout,"\n");
	    fprintf(stdout,"%s ",(const char *)t.string());
	}
    }
    if (in_options)
	fprintf(stdout,"\n");
    
    
}

static void output_sgml_options(const EST_String &usage)
{
    EST_TokenStream ts;
    EST_Token t;
    EST_String atype;
    int in_options = FALSE;
    
    ts.open_string(usage);
    ts.set_SingleCharSymbols("{}[]|");
    ts.set_PunctuationSymbols("");
    ts.set_PrePunctuationSymbols("");

    fprintf(stdout,"<variablelist>\n");
    
    while (!ts.eof())
    {
	t = ts.get();
	if ((t.string().contains("-",0)) &&
	    (t.whitespace().contains("\n")))
	{			// An argument
	    if (in_options)
		fprintf(stdout,"\n</PARA></LISTITEM>\n</varlistentry>\n\n");
	    fprintf(stdout,"<varlistentry><term>%s</term>\n<LISTITEM><PARA>\n", (const char *)t.string());
	    if ((ts.peek().string() == "<string>") ||
		(ts.peek().string() == "<float>") ||
		(ts.peek().string() == "<ifile>") ||
		(ts.peek().string() == "<ofile>") ||
		(ts.peek().string() == "<double>") ||
		(ts.peek().string() == "<int>"))
	    {   // must strip of  the angle brackets to make SGML
		atype = ts.get().string();
		atype.gsub("<","");
		atype.gsub(">","");
		fprintf(stdout,"<replaceable>%s</replaceable>\n",
			(const char *) atype);
	    }
	    if ((ts.peek().string() == "{"))
	    {			// a default value
		ts.get();
		fprintf(stdout," \" {%s}\"",(const char *)ts.get().string());
		ts.get();
	    }
	    if (!ts.peek().whitespace().contains("\n"))
		fprintf(stdout,"\n");
	    in_options = TRUE;
	}
	else if (in_options)
	{
	    if (t.whitespace().contains("\n"))
		fprintf(stdout,"\n");
	    fprintf(stdout,"%s ",(const char *)t.string());
	}
    }
    if (in_options)
	fprintf(stdout,"</PARA></LISTITEM>\n</varlistentry>\n</variablelist>\n");
    
}

static void output_sgml_synopsis(char **argv, const EST_String &usage)
{
    EST_TokenStream ts;
    EST_Token t;
    EST_String atype;
    int in_options = FALSE;
    
    ts.open_string(usage);
    ts.set_SingleCharSymbols("{}[]|");
    ts.set_PunctuationSymbols("");
    ts.set_PrePunctuationSymbols("");

    EST_Pathname full(argv[0]);

    fprintf(stdout,"<cmdsynopsis><command>%s</command>", 
	    (const char *)full.filename());

    fprintf(stdout,"%s",(const char *)ts.get_upto_eoln().string());

    while (!ts.eof())
    {
	t = ts.get();
	if ((t.string().contains("-",0)) &&
	    (t.whitespace().contains("\n")))
	{			// An argument
	    if (in_options)
		fprintf(stdout,"</arg>\n");
	    fprintf(stdout,"<arg>%s ", (const char *)t.string());
	    if ((ts.peek().string() == "<string>") ||
		(ts.peek().string() == "<float>") ||
		(ts.peek().string() == "<ifile>") ||
		(ts.peek().string() == "<ofile>") ||
		(ts.peek().string() == "<double>") ||
		(ts.peek().string() == "<int>"))
	    {   // must strip of  the angle brackets to make SGML
		atype = ts.get().string();
		atype.gsub("<","");
		atype.gsub(">","");
		fprintf(stdout,"<replaceable>%s</replaceable>",
			(const char *) atype);
	    }
	    if ((ts.peek().string() == "{"))
	    {			// a default value
		ts.get();
		fprintf(stdout," \" {%s}\"",(const char *)ts.get().string());
		ts.get();
	    }
	    in_options = TRUE;
	}
    }
    fprintf(stdout,"</arg>\n</cmdsynopsis>\n");
}

EST_String options_general(void)
{
    // The standard waveform input options 
    return
	EST_String("")+
	    "-o <ofile>      output file" +
		"-otype <string> output file type\n";
}

void option_override(EST_Features &op, EST_Option al, 
		     const EST_String &option, const EST_String &arg)
{
    if (al.present(arg))
	op.set(option, al.val(arg));
}

