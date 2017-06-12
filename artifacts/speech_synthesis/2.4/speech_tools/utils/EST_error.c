 /************************************************************************/
 /*                                                                      */
 /*                Centre for Speech Technology Research                 */
 /*                     University of Edinburgh, UK                      */
 /*                       Copyright (c) 1996,1997                        */
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
 /*************************************************************************/
 /*                                                                       */
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             */
 /*                   Date: Thu Aug 14 1997                               */
 /* --------------------------------------------------------------------  */
 /* Fatal error calls.                                                    */
 /*                                                                       */
 /*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "EST_error.h"

const char * EST_error_where=NULL;
static char EST_error_message_buf[MAX_ERROR_MESSAGE_LENGTH];
char *EST_error_message = EST_error_message_buf;

void EST_default_bug_fn(const char *format, ...)
{
    va_list ap;
    char *p=EST_error_message;

    if (EST_error_stream==NULL)
      EST_error_stream = stderr;

    fprintf(EST_error_stream, "-=-=-=-=-=- EST Bug! -=-=-=-=-=-\n");
    if (EST_error_where)
      fprintf(EST_error_stream,"    %s\n", EST_error_where);

    va_start(ap, format);
    vsprintf(p, format, ap);
    va_end(ap);
    fprintf(EST_error_stream, "%s\n", p);

    fprintf(EST_error_stream, "Please report this in as much detail as possible\n to festival@cstr.ed.ac.uk\n");
    putc('\n', EST_error_stream);
    fprintf(EST_error_stream, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
    est_error_throw();
}

void EST_default_error_fn(const char *format, ...)
{
    va_list ap;
    char *p=EST_error_message;

    if (EST_error_stream==NULL)
      EST_error_stream = stderr;

    fprintf(EST_error_stream, "-=-=-=-=-=- EST Error -=-=-=-=-=-\n");
    if (EST_error_where)
      fprintf(EST_error_stream,"    %s\n", EST_error_where);

    va_start(ap, format);
    vsprintf(p, format, ap);
    va_end(ap);
    fprintf(EST_error_stream, "%s\n", p);

    fprintf(EST_error_stream, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
    est_error_throw();
}

void EST_quiet_error_fn(const char *format, ...)
{
    va_list ap;
    char *p=EST_error_message;

    va_start(ap, format);
    vsprintf(p, format, ap);
    va_end(ap);

    est_error_throw();
}

void EST_default_warning_fn(const char *format, ...)
{
    va_list ap;
    char *p=EST_error_message;

    if (EST_warning_stream==NULL)
      EST_warning_stream = stderr;

    fprintf(EST_warning_stream, "-=-=-=-=-=- EST Warning -=-=-=-=-=-\n");
    if (EST_error_where)
      fprintf(EST_warning_stream,"    %s\n", EST_error_where);

    va_start(ap, format);
    vsprintf(p, format, ap);
    va_end(ap);
    fprintf(EST_warning_stream, "%s\n", p);

    fprintf(EST_warning_stream, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
}

void EST_quiet_warning_fn(const char *format, ...)
{
    va_list ap;
    char *p=EST_error_message;

    va_start(ap, format);
    vsprintf(p, format, ap);
    va_end(ap);
}

void EST_default_sys_error_fn(const char *format, ...)
{
    va_list ap;
    char *p=EST_error_message;
    const char *msg = strerror(errno);

    if (EST_error_stream==NULL)
      EST_error_stream = stderr;

    fprintf(EST_error_stream, "-=-=-=-=-=- EST IO Error -=-=-=-=-\n");
    if (EST_error_where)
      fprintf(EST_error_stream,"    %s\n", EST_error_where);

    va_start(ap, format);
    vsprintf(p, format, ap);
    va_end(ap);
    fprintf(EST_error_stream, "%s - %s\n", p, msg);

    fprintf(EST_error_stream, "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n");
    est_error_throw();
}

void EST_quiet_sys_error_fn(const char *format, ...)
{
    va_list ap;
    char *p=EST_error_message;
    const char *msg = strerror(errno);

    va_start(ap, format);
    vsprintf(p, format, ap);
    va_end(ap);

    while (*p)
      ++p;
    strcat(p, " - ");
    p+=3;
    strcat(p, msg);

    est_error_throw();
}

void EST_errors_default()
{
  EST_bug_func = EST_default_bug_fn;
  EST_error_func = EST_default_error_fn;
  EST_sys_error_func = EST_default_sys_error_fn;
}

void EST_errors_quiet()
{
  EST_bug_func = EST_default_bug_fn;
  EST_error_func = EST_quiet_error_fn;
  EST_sys_error_func = EST_quiet_sys_error_fn;
}

EST_error_handler EST_bug_func = EST_default_bug_fn;
EST_error_handler EST_error_func = EST_default_error_fn;
EST_error_handler EST_sys_error_func = EST_default_sys_error_fn;
EST_error_handler EST_warning_func = EST_default_warning_fn;

EST_error_handler old_error_function;
EST_error_handler old_sys_error_function;

FILE *EST_error_stream=NULL;
FILE *EST_warning_stream=NULL;

jmp_buf *est_errjmp = 0;
long errjmp_ok=0;





