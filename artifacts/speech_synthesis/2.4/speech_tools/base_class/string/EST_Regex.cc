 /************************************************************************/
 /*                                                                      */
 /*                Centre for Speech Technology Research                 */
 /*                     University of Edinburgh, UK                      */
 /*                        Copyright (c) 1997                            */
 /*                        All Rights Reserved.                          */
 /*                                                                      */
 /*  Permission is hereby granted, free of charge, to use and distribute */
 /*  this software and its documentation without restriction, including  */
 /*  without limitation the rights to use, copy, modify, merge, publish, */
 /*  distribute, sublicense, and/or sell copies of this work, and to     */
 /*  permit persons to whom this work is furnished to do so, subject to  */
 /*  the following conditions:                                           */
 /*   1. The code must retain the above copyright notice, this list of   */
 /*      conditions and the following disclaimer.                        */
 /*   2. Any modifications must be clearly marked as such.               */
 /*   3. Original authors' names are not deleted.                        */
 /*   4. The authors' names are not used to endorse or promote products  */
 /*      derived from this software without specific prior written       */
 /*      permission.                                                     */
 /*                                                                      */
 /*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK       */
 /*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING     */
 /*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT  */
 /*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE    */
 /*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES   */
 /*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN  */
 /*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,         */
 /*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF      */
 /*  THIS SOFTWARE.                                                      */
 /*                                                                      */
 /************************************************************************/
 /*	       Author : Richard Caley (rjc@cstr.ed.ac.uk)		 */
 /*                 Date  : February 1997                                */
 /* -------------------------------------------------------------------- */
 /*                                                                      */
 /* A Regular expression class to go with the CSTR EST_String class. Uses*/
 /* Henry Spencer`s regexp routines which allocate space dynamically     */
 /* using malloc, so we use free in here rather than wfree because       */
 /* wfree might at some time start doing something more than just be a   */
 /* safe wrapper around free. If you try and use another regexp          */
 /* package, beware of changes to how memory is allocated.               */
 /*                                                                      */
 /* We maintain two compiled versions, one for substring matches and     */
 /* one for whole string matches (because sometimes the regexp           */
 /* compiler can special case the latter). These are compiled when       */
 /* first used.                                                          */
 /*                                                                      */
 /************************************************************************/

#ifdef NO_EST
#    include <unistd.h>
#else
#    include "EST_unix.h"
#endif
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "EST_String.h"
#include "EST_Regex.h"

#ifdef sun
#ifndef __svr4__
/* SunOS */
#include <cstring>
#endif
#endif

// extern "C" {
#include "regexp.h"

/*
void *t_regcomp(void *v)
{
  return v;
}

void *cpp_regcomp(void *v)
{
  return v;
}
*/
// #define wfree(P) (1==1)

// These define the different escape conventions for the FSF's
// regexp code and Henry Spencer's

static const char *fsf_magic="^$*+?[].\\";
static const char *fsf_magic_backslashed="()|<>";
static const char *spencer_magic="^$*+?[].()|\\\n";
static const char *spencer_magic_backslashed="<>";

EST_Regex RXwhite("[ \n\t\r]+");
EST_Regex RXalpha("[A-Za-z]+");
EST_Regex RXlowercase("[a-z]+");
EST_Regex RXuppercase("[A-Z]+");
EST_Regex RXalphanum("[0-9A-Za-z]+");
EST_Regex RXidentifier("[A-Za-z_][0-9A-Za-z_]+");
EST_Regex RXint("-?[0-9]+");
EST_Regex RXdouble("-?\\(\\([0-9]+\\.[0-9]*\\)\\|\\([0-9]+\\)\\|\\(\\.[0-9]+\\)\\)\\([eE][---+]?[0-9]+\\)?");

// use this to free compiled regex since the regexp package uses malloc
// and walloc might end up doing something clever.

/* extern "C" void free(void *p); */

#if NSUBEXP != EST_Regex_max_subexpressions
#   error "EST_Regex_max_subexpressions must be equal to  NSUBEXP" 
#endif

EST_Regex::EST_Regex(void) : EST_String()
{
  compiled = NULL;
  compiled_match = NULL;
}

EST_Regex::EST_Regex(const char *s) : EST_String(s)

{

  compiled = NULL;
  compiled_match = NULL;

}

EST_Regex::EST_Regex(EST_String s) : EST_String(s)

{
  compiled = NULL;
  compiled_match = NULL;
}

EST_Regex::EST_Regex(const EST_Regex &ex) : EST_String(ex)
{
  compiled = NULL;
  compiled_match = NULL;
}


EST_Regex::~EST_Regex()
{
    if (compiled_match)
      free(compiled_match);
    if (compiled)
      free(compiled);
}

// Convert a regular expression from the external syntax (defined by the
// the FSF library) to the one expected by the regexp routines (which
// say it's V8 syntax).

char *EST_Regex::regularize(int match) const
{
  char *reg = walloc(char, size()*2+3);
  char *r=reg;
  const char *e;
  int magic=0,last_was_bs=0;
  const char * in_brackets=NULL;
  const char *ex = (size()==0)?"":str();

  if (match && *ex != '^')
    *(r++) = '^';

  for(e=ex; *e ; e++)
    {
     if (*e == '\\' && !last_was_bs)
       {
	 last_was_bs=1;
	 continue;
       }

     magic=strchr((last_was_bs?fsf_magic_backslashed:fsf_magic), *e)!=NULL;

     if (in_brackets)
       {
	 *(r++) = *e;
	 if (*e  == ']' && (e-in_brackets)>1)
	   in_brackets=0;
       }
     else if (magic)
       {
	 if (strchr(spencer_magic_backslashed, *e))
	   *(r++) = '\\';

	 *(r++) = *e;
	 if (*e  == '[')
	   in_brackets=e;
       }
     else 
       {
	 if (strchr(spencer_magic, *e))
	     *(r++) = '\\';

	 *(r++) = *e;
       }
     last_was_bs=0;
    }
  
  if (match && (e==ex || *(e-1) != '$'))
    {
      if (last_was_bs)
	*(r++) = '\\';
      *(r++) = '$';
    }

  *r='\0';

  //  cerr<<"reg||"<<ex<<"||"<<reg<<"\n";
  
  return reg;
}

void EST_Regex::compile()
{
  if (!compiled)
    {
      char *reg=regularize(0);
      void * t =(void *)hs_regcomp(reg);
      compiled=t;
      wfree(reg);
    }

  if (!compiled)
    cerr << "EST_Regex: can't compile '" << str() << "'\n";
}

void EST_Regex::compile_match()
{
  if (!compiled_match)
    {
      char *reg=regularize(1);

      void * t =(void *)hs_regcomp(reg);
      compiled_match=t;
      wfree(reg);
    }

  if (!compiled_match)
      cerr << "EST_Regex: can't compile '" << str() << "'\n";
}

int EST_Regex::run(const char *on, int from, int &start, int &end, int *starts, int *ends) 
{

  compile();

  if (compiled && from <= (int)strlen(on))
    {
      if (hs_regexec((hs_regexp *)compiled, on+from))
	{
	  hs_regexp *re = (hs_regexp *)compiled;

	  start = re->startp[0] - on;
	  end   = re->endp[0]- on;

	  if (starts)
	    {
	      int i;
	      for (i=0; i<EST_Regex_max_subexpressions; i++)
		starts[i] = re->startp[i]?(re->startp[i] - on):-1;
	    }
	  if (ends)
	    {
	      int i;
	      for (i=0; i<EST_Regex_max_subexpressions; i++)
		  ends[i] = re->endp[i]?(re->endp[i] - on):-1;
	    }

	  return 1;
	}
    }
  return 0;
}

int EST_Regex::run_match(const char *on, int from, int *starts, int *ends) 
{

  compile_match();

  hs_regexp *re = (hs_regexp *)compiled_match;

  if (compiled_match && from <= (int)strlen(on))
    if (hs_regexec(re, on+from))
      {
	  if (starts)
	    {
	      int i;
	      for (i=0; i<EST_Regex_max_subexpressions; i++)
		starts[i] = re->startp[i]?(re->startp[i] - on):-1;
	    }
	  if (ends)
	    {
	      int i;
	      for (i=0; i<EST_Regex_max_subexpressions; i++)
		ends[i] = re->endp[i]?(re->endp[i] - on):-1;
	    }
	  return 1;
      }

  return 0;
}

EST_Regex &EST_Regex::operator = (const EST_Regex ex)
{
  ((EST_String &)(*this)) = (EST_String)ex;
  compiled = NULL;
  compiled_match = NULL;

  return *this;
}

EST_Regex &EST_Regex::operator = (const EST_String s)
{
  ((EST_String &)(*this)) = s;
  compiled = NULL;
  compiled_match = NULL;

  return *this;
}

EST_Regex &EST_Regex::operator = (const char *s)
{
  ((EST_String &)(*this)) = s;
  compiled = NULL;
  compiled_match = NULL;

  return *this;
}

ostream &operator << (ostream &s, const EST_Regex &str)
{
  return s << (EST_String)str;
}

