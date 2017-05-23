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
/*                     Date   :  December 1996                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/*  A format function for formated output (like printf)                  */
/*                                                                       */
/*  Its amazing how much I have to write myself to get this to work when */
/*  most people believe this a c library function.                       */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <cstdio>
#include "EST_cutils.h"
#include "siod.h"
#include "siodp.h"

static int format_string(LISP fd,const char *formatstr, const char *str);
static int format_lisp(LISP fd,const char *formatstr, LISP a);
static int format_int(LISP fd,const char *formatstr, int i);
static int format_float(LISP fd,const char *formatstr, float f);
static int format_double(LISP fd,const char *formatstr, double d);
static int format_char(LISP fd, char c);
static int get_field_width(const char *directive);
static char *get_directive(const char *fstr);
static char directive_type(const char *fstr);
static void output_string(LISP fd,const char *str);
static int count_arg_places(const char *formatstring);

static EST_String outstring;
static EST_Regex anumber_rx("[0-9]+");

LISP l_format(LISP args)
{
    // A format function for formated output 
    // Hmm not sure how to do this without writing lots myself
    const char *formatstring = get_c_string(car(cdr(args)));
    LISP lfd = car(args);
    LISP fargs = cdr(cdr(args));
    int i;
    LISP a;

    if (count_arg_places(formatstring) != siod_llength(fargs))
	err("format: wrong number of args for format string",NIL);

    outstring="";

    for (i=0,a=fargs; formatstring[i] != '\0'; i++)
    {
	if (formatstring[i] != '%')
	    format_char(lfd,formatstring[i]);
	else if (formatstring[i+1] == '%')
	{
	    format_char(lfd,formatstring[i]);
	    i++;    // skip quoted %
	}
	else if (directive_type(formatstring+i) == 's')
	{
	    i+= format_string(lfd,formatstring+i,get_c_string(car(a)));
	    a = cdr(a);
	}
	else if (directive_type(formatstring+i) == 'l')
	{
	    i+= format_lisp(lfd,formatstring+i,car(a));
	    a = cdr(a);
	}
	else if ((directive_type(formatstring+i) == 'd') ||
		 (directive_type(formatstring+i) == 'x'))
	{
	    i += format_int(lfd,formatstring+i,(int)get_c_int(car(a)));
	    a = cdr(a);
	}
	else if (directive_type(formatstring+i) == 'f')
	{
	    i += format_float(lfd,formatstring+i,(float)get_c_double(car(a)));
	    a = cdr(a);
	}
	else if (directive_type(formatstring+i) == 'g')
	{
	    i += format_double(lfd,formatstring+i,get_c_double(car(a)));
	    a = cdr(a);
	}
	else if (directive_type(formatstring+i) == 'c')
	{
	    format_char(lfd,(char)get_c_int(car(a)));
	    i++;
	    a = cdr(a);
	}
	else
	{
	    cerr << "SIOD format: unsupported format directive %"
		<< directive_type(formatstring+i) << endl;
	    err("",NIL);
	}
    }

    if (lfd == NIL)
	return strintern(outstring);
    else
	return NIL;
}

static int format_string(LISP fd,const char *formatstr, const char *str)
{
    // Output str to fd using directive at start of formatstr
    // Returns the number character in the format directive
    char *directive = get_directive(formatstr);
    int width = get_field_width(directive);
    char *buff;

    if (width > (signed)strlen(str))
	buff = walloc(char,width+10);
    else
	buff = walloc(char,strlen(str)+1);

    sprintf(buff,directive,str);

    output_string(fd,buff);
    width = strlen(directive)-1;
    wfree(buff);
    wfree(directive);
    
    return width;
}

static int format_lisp(LISP fd,const char *formatstr, LISP a)
{
    // Output a as str to fd using directive at start of formatstr
    // Returns the number character in the format directive
    char *directive = get_directive(formatstr);
    int width = get_field_width(directive);
    EST_String buff;

    if (width != 0)
	err("format: width in %l not supported",NIL);

    buff = siod_sprint(a);

    output_string(fd,buff);
    width = strlen(directive)-1;
    wfree(directive);
    
    return width;
}

static int format_int(LISP fd, const char *formatstr, int i)
{
    // Output i to fd using directive at start of formatstr
    // Returns the number character in the format directive
    char *directive = get_directive(formatstr);
    int width = get_field_width(directive);
    char *buff;

    if (width > 20)
	buff = walloc(char,width+10);
    else
	buff = walloc(char,20);

    sprintf(buff,directive,i);

    output_string(fd,buff);
    width = strlen(directive)-1;
    wfree(buff);
    wfree(directive);
    
    return width;
}

static int format_float(LISP fd, const char *formatstr, float f)
{
    // Output f to fd using directive at start of formatstr
    // Returns the number character in the format directive
    char *directive = get_directive(formatstr);
    int width = get_field_width(directive);
    char *buff;

    if (width > 20)
	buff = walloc(char,width+10);
    else
	buff = walloc(char,20);

    sprintf(buff,directive,f);

    output_string(fd,buff);
    width = strlen(directive)-1;
    wfree(buff);
    wfree(directive);
    
    return width;
}

static int format_double(LISP fd, const char *formatstr, double d)
{
    // Output f to fd using directive at start of formatstr
    // Returns the number character in the format directive
    char *directive = get_directive(formatstr);
    int width = get_field_width(directive);
    char *buff;

    if (width > 30)
	buff = walloc(char,width+10);
    else
	buff = walloc(char,30);

    sprintf(buff,directive,d);

    output_string(fd,buff);
    width = strlen(directive)-1;
    wfree(buff);
    wfree(directive);
    
    return width;
}

static int format_char(LISP fd, char c)
{
    // Output c to fd using directive at start of formatstr
    // Returns the number character in the format directive
    char buff[10];

    sprintf(buff,"%c",c);

    output_string(fd,buff);
    
    return 0;
}

static int get_field_width(const char *directive)
{
    // Look inside the directive for any explicit width info
    
    if (strlen(directive) == 2)
	return 0;
    else 
    {
	EST_String nums = directive;
	nums = nums.at(1,strlen(directive)-2);
	if (nums.matches(anumber_rx))
	    return atoi(nums);
	else if (nums.contains("."))
	{
	    EST_String n1 = nums.before(".");
	    EST_String n2 = nums.after(".");
	    return atoi(n1) + atoi(n2);
	}
	else
	{
	    cerr << "SIOD format: can't find width in directive "
		<< directive << endl;
	    err("",NIL);
	}
    }
    return 0;
}

static char *get_directive(const char *fstr)
{
    // Copy the format directive from the start of this string
    int i;

    for (i=0; fstr[i] != '\0'; i++)
	if ((fstr[i] >= 'a') &&
	    (fstr[i] <= 'z'))
	    break;
    if (fstr[i] == '\0')
	err("format: premature end of format structure",NIL);
    char *direct = walloc(char,i+2);
    memmove(direct,fstr,i+1);
    direct[i+1] = '\0';
    return direct;
}

static char directive_type(const char *fstr)
{
    // return the next lower case character.  This identifies the
    // type of the argument to be inserted in the format string
    int i;
    
    for (i=0; fstr[i] != '\0'; i++)
	if ((fstr[i] >= 'a') &&
	    (fstr[i] <= 'z'))
	{
	    return fstr[i];
	}

    err("SIOD format: premature end of format structure",NIL);
    return '\0';

}

static void output_string(LISP fd, const char *str)
{
    if (fd == NIL)
	outstring += str;
    else if (fd == truth)
	fprintf(stdout,"%s",str);
    else if (TYPEP(fd,tc_c_file))
	fprintf(get_c_file(fd,NULL),"%s",str);
    else
	err("format: not a file",fd);
}

static int count_arg_places(const char *formatstring)
{
    // count number of places in the format string.
    int c,i;

    for (c=i=0; formatstring[i] != '\0'; i++)
	if (formatstring[i] == '%')
	{
	    if (formatstring[i+1] == '%')
		i++;
	    else
		c++;
	}

    return c;
}

void init_subrs_format()
{
 init_lsubr("format",l_format,
 "(format FD FORMATSTRING ARG0 ARG1 ...)\n\
  Output ARGs to FD using FROMATSTRING.  FORMATSTRING is like a printf\n\
  formatstrng.  FD may be a filedescriptor, or t (standard output) or\n\
  nil (return as a string).  Note not all printf format directive are\n\
  supported.  %l is additionally support for Lisp objects.\n\
  [see Scheme I/O]");
}
