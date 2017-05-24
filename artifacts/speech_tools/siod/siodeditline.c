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
/*                     Author :  Alan W Black                            */
/*                     Date   :  December 1998                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/*  Due to incompatibility between the GPL for readline and Festival's   */
/*  current licence readline support was removed after 1.3.0             */
/*  This uses a much simpler but much poorer command line editor called  */
/*  editline instead.                                                    */
/*                                                                       */
/*  Although this code is included in our distribution we still offer    */
/*  optional compilation as it may not work on all platforms             */
/*                                                                       */
/*=======================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include "EST_unix.h"
#include <string.h>
#include "EST_cutils.h"
#include "siodeditline.h"

FILE *stddebug = NULL;
extern int el_pos;
extern char *repl_prompt;

#ifndef SUPPORT_EDITLINE

/* If for some reason you don't want editline the following */
/* functions are provided.  They are functional but minimal.  They are */
/* suitable when you are in an embedded environment and don't need     */
/* command lin editing                                                 */
int el_no_echo;
int editline_histsize;

int siod_el_getc(FILE *f)
{
    int c;

    if (el_pos == -1)
    {
	fprintf(stdout,"%s",repl_prompt);
	fflush(stdout);
	el_pos = 0;
    }

    c = getc(f);

    if (c == '\n')
	el_pos = -1;
	
    return c;
}

void siod_el_ungetc(int c, FILE *f)
{
    ungetc(c,f);
}

void siod_el_init(void)
{
    return;
}
#else
#include "editline.h"

static int possible_commandp(char *text, int start, int end);
static int possible_variablep(char *text, int start, int end);
static char **command_completion (char *text,int start,int end);

static char *el_line = NULL;

char *editline_history_file = ".editline_history";
static char *full_history_file = ".editline_history";

static STATUS siod_display_doc ()
{
    /* Find the current symbol and check for a documentation string */
    char *symbol;
    const char *docstring;
    int i;

    symbol = el_current_sym();
    putc('\n',stderr);
    docstring = siod_docstring(symbol);
    for (i=0; docstring[i] != '\0'; i++)
	putc(docstring[i],stderr);
    putc('\n',stderr);
    fflush(stderr);
    wfree(symbol);
    el_redisplay();
    return CSmove;
}

static STATUS siod_say_doc ()
{
    /* Find the current symbol and check for a documentation string */
    /* Now this is what you call wasting your time.  Here we get the */
    /* synthesizer to say the documentation string                   */
    char *symbol;

    symbol = el_current_sym();
    fprintf(stderr,"\nsynthesizing doc string ...");
    fflush(stderr);
    siod_saydocstring(symbol);
    putc('\n',stderr);
    fflush(stderr);
    wfree(symbol);
    el_redisplay();
    return CSmove;
}

static STATUS siod_manual()
{
    /* Find the current symbol and check for a documentation string    */
    /* Look for a "see " reference in its documentation string, if so  */
    /* access that section of the manual by sending a call to netscape */
    char *symbol;
    const char *infostring;

    symbol = el_current_sym();
    infostring = siod_manual_sym(symbol);
    putc('\n',stderr);
    fprintf(stderr,"%s",infostring);
    fflush(stderr);
    putc('\n',stderr);
    fflush(stderr);
    el_redisplay();
    wfree(symbol);
    return CSmove;
}

void siod_el_init(void)
{
    /* Various initialization completion, history etc */
    char *home;

    home = getenv("HOME");
    if (home==NULL)
      home="";
    full_history_file = 
	walloc(char,strlen(home)+strlen(editline_history_file)+2);
    sprintf(full_history_file,"%s/%s",home,editline_history_file);
    read_history(full_history_file);
    el_user_intr = TRUE;  /* we want SIGINT to raise a signal */

    el_user_completion_function = command_completion;
    el_bind_key_in_metamap('h',siod_display_doc);
    el_bind_key_in_metamap('s',siod_say_doc);
    el_bind_key_in_metamap('m',siod_manual);
}

int siod_el_getc(FILE *f)
{
    int c;

    if (el_pos == -1)
    {
	el_line=readline(repl_prompt);
	if (el_line != NULL)
	{
	    add_history(el_line);
	    write_history(full_history_file);
	}
	el_pos = 0;
    }
    if ((el_line==NULL) ||
	(strlen(el_line) <= el_pos))
	el_pos = -1;
    if (el_line==NULL)
	c = EOF;
    else if (el_pos == -1)
	c = '\n';   /* whitespace representing end of line */
    else 
    {
	c = el_line[el_pos];
	el_pos++;
    }

    return c;
}

void siod_el_ungetc(int c, FILE *f)
{
    if (el_pos > 0)
	el_pos--;
    else
    {
	fprintf(stderr,"fix ungetc when nothing is there");
    }
}
	
static int qsort_str_compare(const void *p1,const void *p2)
{
    const char	*s1;
    const char	*s2;

    s1 = *(const char **)p1;
    s2 = *(const char **)p2;

    return strcmp(s1,s2);
}

static char **command_completion (char *text,int start,int end)
{
    char **matches = NULL;
    int i;

    /* If preceding non-alphanum character is a left paren, */
    /* look for a command else look for any variable */
    if (possible_commandp(text,start,end))
	matches = siod_command_generator(text+start,end-start);
    else if (possible_variablep(text,start,end))
	matches = siod_variable_generator(text+start,end-start);

    if (matches && matches[0] && matches[1])
    {
	/* If there are at least two, Sort them  */
	for (i=0; matches[i] != NULL; i++);
	qsort(matches,i,sizeof(char **),qsort_str_compare);
    }

    return matches;
}

static int possible_commandp(char *text, int start, int end)
{
    /* If non-white space previous to this is a left paren */
    /* signal we are looking for a function name           */
    int t;

    for (t=start-1; t >= 0; t--)
	if (strchr(" \t\n\r",text[t]) != NULL)
	    continue;
	else if (text[t] == '(')
	    return TRUE;
	else
	    return FALSE;

    return FALSE;
}

static int possible_variablep(char *text, int start, int end)
{
    /* Almost negative of above but if previous symbol is a quote */
    /* let the file completion stuff do it                        */
    int t;

    for (t=start-1; t >= 0; t--)
	if (strchr(" \t\n",text[t]) != NULL)
	    continue;
	else if (text[t] == '(')
	    return FALSE;
	else if ((text[t] == '"') &&
		 (t == start-1))
	    return FALSE;
	else
	    return TRUE;

    return TRUE;
}

#endif /* SUPPORT_EDITLINE */
