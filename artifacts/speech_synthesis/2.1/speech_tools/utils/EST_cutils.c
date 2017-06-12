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
/*                 Author :  Alan W Black and Paul Taylor                */
/*                 Date   :  July 1996                                   */
/*-----------------------------------------------------------------------*/
/*               Various Low level C utilities                           */
/*                                                                       */
/*=======================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "EST_unix.h"
#include <string.h>
#include "EST_cutils.h"

#define _S_S_S(S) #S
#define STRINGIZE(S) _S_S_S(S)

const char * const est_tools_version = 
     STRINGIZE(ESTVERSION) ":" STRINGIZE(ESTSTATE) " " STRINGIZE(ESTDATE) ;

const char * const est_name = STRINGIZE(ESTNAME);

#ifdef ESTLIBDIRC
#    define ESTLIBDIR STRINGIZE(ESTLIBDIRC)
#endif

#ifndef ESTLIBDIR
#define ESTLIBDIR "/usr/local/lib/speech_tools"
#endif

const char * const est_libdir = ESTLIBDIR;

const char * const est_ostype = STRINGIZE(ESTOSTYPE);

char *cmake_tmp_filename()
{
    char *tdir;
    char fname[1024];
    static int n=0;
    char *t1;
    int i,j;

    if (((tdir=getenv("TMPDIR")) == NULL) &&
	((tdir=getenv("TEMP")) == NULL) &&
	((tdir=getenv("TMP")) == NULL))
	tdir = "/tmp";
    
    t1 = wstrdup(tdir);

    /* If it contains a double quote there might be a security hole */
    for (j=i=0; t1[i] != '\0'; i++)
	if (t1[i] != '"')
	    t1[j++]=t1[i];

    sprintf(fname,"%s/est_%05ld_%05d",t1,(long)getpid(),n++);
    return wstrdup(fname);
}

enum EST_bo_t str_to_bo(const char *boname)
{
    /* EST_String to byte order */

    if ((streq(boname,"hilo")) || (streq(boname,"big")) || 
	(streq(boname,"MSB")) ||  (streq(boname,"big_endian")))
	return bo_big;
    else if ((streq(boname,"lohi")) || (streq(boname,"little")) || 
	     (streq(boname,"LSB")) ||  (streq(boname,"little_endian")))
	return bo_little;
    else if ((streq(boname,"native")) || (streq(boname,"mine")))
	return (EST_BIG_ENDIAN ? bo_big : bo_little);
    else if ((streq(boname,"nonnative")) || (streq(boname,"other")) ||
	     (streq(boname,"wrong")) || (streq(boname,"swap")) ||
	     (streq(boname,"swapped")))
	return (EST_BIG_ENDIAN ? bo_little : bo_big);
    else
    {
	fprintf(stderr,"Unknown byte swap format: \"%s\" assuming native\n",
		boname);
	return (EST_BIG_ENDIAN ? bo_big : bo_little);
    }

}    

const char *bo_to_str(enum EST_bo_t bo)
{
    /* byte order to str */

    switch (bo)
    {
      case bo_big:    return "hilo";
      case bo_little: return "lohi";
      default:
	fprintf(stderr,"Unrecognized byte order %d\n",bo);
	return "unrecognized";
    }
}

