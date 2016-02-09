/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                       Copyright (c) 1996,1997                         */
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
/*                Author :  Alan W Black and Paul Taylor                 */
/*                Date   :  April 1996                                   */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Top level file for synthesizer                                        */
/*                                                                       */
/*=======================================================================*/
#include <cstdio>
#include "EST_unix.h"
#include "EST_Pathname.h"
#include <cstdlib>
#include "festival.h"
#include "festivalP.h"
#include "siod.h"
#include "ModuleDescription.h"

void festival_lisp_funcs(void);
void festival_lisp_vars(void);
void festival_banner(void);
void festival_load_default_files(void);

#define _S_S_S(S) #S
#define STRINGIZE(S) _S_S_S(S)

const char *festival_version =  STRINGIZE(FTVERSION) ":" STRINGIZE(FTSTATE) " " STRINGIZE(FTDATE);

// Allow the path to be passed in without quotes because Windoze command line
// is stupid
// Extra level of indirection needed to get an extra macro expansion. Yeuch.

#ifdef FTLIBDIRC
#    define FTLIBDIR STRINGIZE(FTLIBDIRC)
#endif
#ifdef FTOSTYPEC
#    define FTOSTYPE STRINGIZE(FTOSTYPEC)
#endif

#ifndef FTLIBDIR
#define FTLIBDIR "/projects/festival/lib/"
#endif
#ifndef FTOSTYPE
#define FTOSTYPE ""
#endif

const char *festival_libdir = FTLIBDIR;
ostream *cdebug;
static int festival_server_port = 1314;
static EST_StrList sub_copyrights;

#if 0
LISP describe_module(LISP lname, LISP lstream);
LISP describe_all_modules(void);
#endif

static int festival_initialized = 0;

void festival_initialize(int load_init_files,int heap_size)
{
    // all initialisation

    if (! festival_initialized )
    {
	siod_init(heap_size);
	siod_est_init();		// add support for EST objects
	siod_fringe_init();		// and for talking to fringe.
	
	siod_prog_name = "festival";
	cdebug = new ofstream("/dev/null");  // This wont work on Win/NT
	stddebug = fopen("/dev/null","w");
	
	festival_lisp_vars();
	festival_lisp_funcs();
	if (load_init_files)
	    festival_load_default_files();
	festival_initialized = TRUE;
    }
    else
    {
	cerr << "festival_initialize() called more than once" << endl;
    }

    return;
}

void festival_repl(int interactive)
{
    // top level read eval print loop

    siod_primary_prompt = "festival> ";
    siod_secondary_prompt = "> ";
    if (interactive)
	festival_banner();
    siod_repl(interactive);  

}

void festival_server_mode(void)
{
    // Go into server mode 
    LISP lport;

    lport = siod_get_lval("server_port",NULL);

    if (lport != NULL)
	festival_server_port = get_c_int(lport);

    festival_start_server(festival_server_port);

}

void festival_init_lang(const EST_String &language)
{
    // Call the lisp function to setup for names language 
    
    leval(cons(rintern("select_language"),
	       cons(quote(rintern(language)),NIL)),
	  NIL);
}

int festival_load_file(const EST_String &fname)
{
    // Load and evaluate named file
    EST_String b;
    b = EST_String("(load ")+quote_string(fname,"\"","\\",1)+")";
    // I used to do the above without the b intermediate variable
    // but that caused a crash for some compilers on some machines 
    return festival_eval_command(b);
}

int festival_eval_command(const EST_String &command)
{
    // Eval command but catch any errors
    // catch any errors and return true or false depending on success
    jmp_buf *old_errjmp = est_errjmp;
    int old_errjmp_ok = errjmp_ok;
    int rvalue = TRUE;
    LISP l;
    gc_protect(&l);

    errjmp_ok = 1;
    est_errjmp = walloc(jmp_buf,1);
    if (setjmp(*est_errjmp)) 
    {    
	rvalue = FALSE;
    }
    else
    {
	EST_String ll = command;  // copy it;
	l = read_from_string((char *)ll);
	leval(l,NIL); 
	rvalue = TRUE;
    }

    gc_unprotect(&l);
    // Restore error handler 
    wfree(est_errjmp); 
    est_errjmp = old_errjmp;  
    errjmp_ok = old_errjmp_ok;
    return rvalue;
}

void festival_tidy_up(void)
{
    // tidy up before exit

    // specifically this closes any ope files, which can be useful
    // if finding errors
    siod_tidy_up();  
    
}

void festival_banner(void)
{
    /* Print out a banner, copyright and version */
    
    if (siod_get_lval("hush_startup",NULL) == NIL)
    {
	EST_Litem *t;
	cout << "\n" << STRINGIZE(FTNAME) << " " << 
	    festival_version << endl;
	cout << "Copyright (C) University of Edinburgh, 1996-2010. " <<
	    "All rights reserved." << endl;
	if (sub_copyrights.length() > 0)
        {
            cout << "\n";
	    for (t = sub_copyrights.head(); t != 0; t = t->next())
		cout << sub_copyrights.item(t);
        }
	cout << "For details type `(festival_warranty)'" << endl;
    }
}

void festival_def_utt_module(const char *name,LISP (*fcn)(LISP),
			     const char *docstring)
{
    // Define an utterance module to the LISP system
    init_subr_1(name,fcn,docstring);
}

int festival_say_file(const EST_String &fname)
{
    /* Say this file as text */
    // should use pathname on this
    return festival_eval_command(EST_String("(tts ")+
					    quote_string(fname,"\"","\\",1)+
					    " nil)");
}

int festival_say_text(const EST_String &text)
{
    /* Say this text */
    return festival_eval_command(EST_String("(SayText ")+
					    quote_string(text,"\"","\\",1)+
					    ")");
}

int festival_text_to_wave(const EST_String &text,EST_Wave &wave)
{
    /* Convert text to waveform */
    LISP lutt;
    EST_Wave *w;
    
    if (!festival_eval_command(EST_String("(set! wave_utt (SynthText ")+
					    quote_string(text,"\"","\\",1)+
					    "))"))
	return FALSE;
    lutt = siod_get_lval("wave_utt",NULL);
    if (!utterance_p(lutt))
	return FALSE;
    w = get_utt_wave(utterance(lutt));
    if (w == 0)
	return FALSE;
    wave = *w;
    return TRUE;
}

void festival_wait_for_spooler(void)
{
    leval(cons(rintern("audio_mode"),
	       cons(quote(rintern("close")),NIL)),NIL);
}

static LISP lisp_debug_output(LISP arg)
{
    // switch debug output stream

    if (cdebug != &cerr)
	delete cdebug;
    if (stddebug != stderr)
	fclose(stddebug);

    if (arg == NIL)
    {   // this might be a problem on non-Unix machines
	cdebug = new ofstream("/dev/null");
	stddebug = fopen("/dev/null","w");
    }
    else
    {
	cdebug = &cerr;
	stddebug = stderr;
    }

    return NIL;
}

void festival_load_default_files(void)
{
    // Load in default files, init.scm. Users ~/.festivalrc 
    // (or whatever you wish to call it) is loaded by init.scm
    EST_String userinitfile, home_str, initfile;

    // Load library init first
    initfile = (EST_String)EST_Pathname(festival_libdir).as_directory() + 
	"init.scm";
    if (access((const char *)initfile,R_OK) == 0)
	vload(initfile,FALSE);
    else
	cerr << "Initialization file " << initfile << " not found" << endl;

}

void festival_lisp_vars(void)
{
    // set up specific lisp variables 
    EST_TokenStream ts;
    int major,minor,subminor;
    
    siod_set_lval("libdir",strintern(festival_libdir));
    if (!streq(FTOSTYPE,""))
	siod_set_lval("*ostype*",cintern(FTOSTYPE));
    siod_set_lval("festival_version",
		  strcons(strlen(festival_version),festival_version));
    ts.open_string(festival_version);
    ts.set_WhiteSpaceChars(". ");
    major = atoi(ts.get().string());
    minor = atoi(ts.get().string());
    subminor = atoi(ts.get().string());
    ts.close();
    siod_set_lval("festival_version_number",
		  cons(flocons(major),
		       cons(flocons(minor),
			    cons(flocons(subminor),NIL))));
    siod_set_lval("*modules*",NIL);
    siod_set_lval("*module-descriptions*",NIL);
    if (nas_supported)
	proclaim_module("nas");
    if (esd_supported)
	proclaim_module("esd");
    if (sun16_supported)
	proclaim_module("sun16audio");
    if (freebsd16_supported)
	proclaim_module("freebsd16audio");
    if (linux16_supported)
	proclaim_module("linux16audio");
    if (macosx_supported)
	proclaim_module("macosxaudio");
    if (win32audio_supported)
	proclaim_module("win32audio");
    if (mplayer_supported)
	proclaim_module("mplayeraudio");
    
    // Add etc-dir path and machine specific directory etc/$OSTYPE
    char *etcdir = walloc(char,strlen(festival_libdir)+strlen("etc/")+
			  strlen(FTOSTYPE)+3);
    sprintf(etcdir,"%s/etc/%s/",festival_libdir,FTOSTYPE);
    char *etcdircommon = walloc(char,strlen(festival_libdir)+strlen("etc/")+3);
    sprintf(etcdircommon,"%s/etc/",festival_libdir);
    
    //  Modify my PATH to include these directories
    siod_set_lval("etc-path",cons(rintern(etcdir),
				  cons(rintern(etcdircommon),NIL)));
    const char *path = getenv("PATH");
    if (path == 0)
	path = "";
    char *newpath = walloc(char,1024+strlen(path)+strlen(etcdir)+
			   strlen(etcdircommon));
    sprintf(newpath,"PATH=%s:%s:%s",path,etcdir,etcdircommon);
    putenv(newpath);
    
    wfree(etcdir);
    wfree(etcdircommon);
    return;
}

static LISP lmake_tmp_filename(void)
{
    EST_String tfile = make_tmp_filename();
    
    return strintern(tfile);
}

LISP l_wagon(LISP si, LISP tree);
LISP l_lr_predict(LISP si, LISP lr_model);
void festival_unitdb_init(void);
LISP Gen_Viterbi(LISP utt);

LISP utf8_explode(LISP name)
{
    /* return a list of utf-8 characters as strings */
    const unsigned char *xxx = (const unsigned char *)get_c_string(name);
    LISP chars=NIL;
    int i, l=0;
    char utf8char[5];

    for (i=0; xxx[i]; i++)
    {
        if (xxx[i] < 0x80)  /* one byte */
        {
            sprintf(utf8char,"%c",xxx[i]);
            l = 1;
        }
        else if (xxx[i] < 0xe0) /* two bytes */
        {
            sprintf(utf8char,"%c%c",xxx[i],xxx[i+1]);
            i++;
            l = 2;
        }
        else if (xxx[i] < 0xff) /* three bytes */
        {
            sprintf(utf8char,"%c%c%c",xxx[i],xxx[i+1],xxx[i+2]);
            i++; i++;
            l = 3;
        }
        else
        {
            sprintf(utf8char,"%c%c%c%c",xxx[i],xxx[i+1],xxx[i+2],xxx[i+3]);
            i++; i++; i++;
            l = 4;
        }
        chars = cons(strcons(l,utf8char),chars);
    }
    return reverse(chars);

}

void festival_lisp_funcs(void)
{
    // declare festival specific Lisp functions 
    
    // Standard functions
    festival_utterance_init();
    festival_features_init();
    festival_wave_init();
    festival_Phone_init();

    festival_tcl_init();	// does nothing if TCL not selected
    festival_wfst_init();	
    festival_ngram_init();	
    
//    festival_unitdb_init();
    //  General ones
    festival_init_modules();
    
    // Some other ones that aren't anywhere else
    init_subr_1("parse_url", lisp_parse_url,
 "(parse_url URL)\n\
  Split URL into a list (protocol host port path) suitable\n\
  for giving to fopen.");
    init_subr_0("make_tmp_filename",lmake_tmp_filename,
 "(make_tmp_filename)\n\
  Return name of temporary file.");
    init_subr_1("debug_output",lisp_debug_output,
 "(debug_output ARG)\n\
  If ARG is non-nil cause all future debug output to be sent to cerr,\n\
  otherwise discard it (send it to /dev/null).");
    init_subr_1("utf8explode", utf8_explode,
 "(utf8explode utf8string)\n\
  Returns a list of utf-8 characters in given string.");
    
    init_subr_2("wagon",l_wagon,
 "(wagon ITEM TREE)\n\
  Apply the CART tree TREE to ITEM.  This returns the full\n\
  predicted form, you need to extract the value from the returned form\n\
  itself. [see CART trees]");
    init_subr_2("lr_predict",l_lr_predict,
 "(lr_predict ITEM LRMODEL)\n\
  Apply the linear regression model LRMODEL to ITEM in.  This\n\
  returns float value by summing the product of the coeffients and values\n\
  returned by the specified features in ITEM. [see Linear regression]");
    init_subr_1("Gen_Viterbi",Gen_Viterbi,
 "(Gen_Viterbi UTT)\n\
  Applies viterbi search algorithm based on the parameters in\n\
  gen_vit_params.  Basically allows user candidate selection function\n\
  combined with ngrams.");

#if 0
    init_subr_2("describe_module", describe_module,
    "(describe_module NAME STREAM)\n\
  Print a description of the named module.");
    init_subr_0("describe_all_modules", describe_all_modules,
    "(describe_all_modules)\n\
  Print descriptions of all modules.");
#endif
    
}

void proclaim_module(const EST_String &name,
		     const EST_String &banner_copyright,
		     const ModuleDescription *description)
{
    // Add this name to the variable *modules*, so people can test for
    // it in the lisp world 
    LISP mods = siod_get_lval("*modules*",NULL);
    LISP name_sym = rintern(name);
    
    siod_set_lval("*modules*",cons(name_sym,mods));
    
    if (banner_copyright != "")
	sub_copyrights.append(name + ": " + banner_copyright);

    if (description != NULL)
      {
	LISP module_descriptions = siod_get_lval("*module-descriptions*",NULL);
	LISP scheme_desc = siod(description);
	siod_set_lval("*module-descriptions*",
		      cons(cons(name_sym, 
				cons(scheme_desc, NIL)),
			   module_descriptions));
      }
    
}

void proclaim_module(const EST_String &name,
		     const ModuleDescription *description)
{ 
  proclaim_module(name, "", description); 
}

void init_module_subr(const char *name, LISP (*fcn)(LISP), 
		      const ModuleDescription *description)
{
  char * desc_string = NULL;

  if (description)
    {
      EST_String desc(ModuleDescription::to_string(*description));

      desc_string = wstrdup(desc);
    }

  init_lsubr((char *)name, fcn, desc_string);

  // delete desc_string;
}

LISP ft_get_param(const EST_String &pname)
{
    EST_Features &p = Param();

    if (p.present(pname))
	return lisp_val(p.f(pname));
    else
	return NIL;
#if 0
    LISP params,lpair;
    
    params = siod_get_lval("Parameter","no parameters");
    
    lpair = siod_assoc_str(pname,params);
    
    if (lpair == NIL)
	return NIL;
    else
	return car(cdr(lpair));
#endif
}

void print_string(EST_String s)
{
    cout << s << endl;
}

LISP map_pos(LISP posmap, LISP pos)
{
    // Map specified features
    LISP l;

    if (consp(pos) || (pos == NIL))
	return pos;

    for (l=posmap; l != NIL; l=cdr(l))
	if (siod_member_str(get_c_string(pos),car(car(l))) != NIL)
	    return car(cdr(car(l)));
    return pos;
}

EST_String map_pos(LISP posmap, const EST_String &pos)
{
    LISP l;

    for (l=posmap; l != NIL; l=cdr(l))
	if (siod_member_str(pos,car(car(l))) != NIL)
	    return get_c_string(car(cdr(car(l))));
    return pos;
}

