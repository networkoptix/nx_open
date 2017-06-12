/****************************************************************************/
/*                                                                          */
/* Copyright 1992 Simmule Turner and Rich Salz.  All rights reserved.       */
/*                                                                          */
/* This software is not subject to any license of the American Telephone    */
/* and Telegraph Company or of the Regents of the University of California. */
/*                                                                          */
/* Permission is granted to anyone to use this software for any purpose on  */
/* any computer system, and to alter it and redistribute it freely, subject */
/* to the following restrictions:                                           */
/* 1. The authors are not responsible for the consequences of use of this   */
/*    software, no matter how awful, even if they arise from flaws in it.   */
/* 2. The origin of this software must not be misrepresented, either by     */
/*    explicit claim or by omission.  Since few users ever read sources,    */
/*    credits must appear in the documentation.                             */
/* 3. Altered versions must be plainly marked as such, and must not be      */
/*    misrepresented as being the original software.  Since few users       */
/*    ever read sources, credits must appear in the documentation.          */
/* 4. This notice may not be removed or altered.                            */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*  This is a line-editing library, it can be linked into almost any        */
/*  program to provide command-line editing and recall.                     */
/*                                                                          */
/*  Posted to comp.sources.misc Sun, 2 Aug 1992 03:05:27 GMT                */
/*      by rsalz@osf.org (Rich $alz)                                        */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/*  The version contained here has some modifications by awb@cstr.ed.ac.uk  */
/*  (Alan W Black) in order to integrate it with the Edinburgh Speech Tools */
/*  library and Scheme-in-one-defun in particular.  All modifications to    */
/*  to this work are continued with the same copyright above.  That is      */
/*  this version of editline does not have the the "no commercial use"      */
/*  restriction that some of the rest of the EST library may have           */
/*  awb Dec 30 1998                                                         */
/*                                                                          */
/****************************************************************************/
/*  $Revision: 1.2 $
**
**  Internal header file for editline library.
**
*/

/* Explicit configuration for Unix environment which will effectively be */
/* set up by the Speech Tools configuration by this point  -- awb */
#define ANSI_ARROWS
#define HAVE_TCGETATTR
#define HAVE_STDLIB
#define HIDE
#define USE_DIRENT
#define SYS_UNIX
/* will all work without this except long lines */
#define USE_TERMCAP

/*
**  Command status codes (moved from editline.h).
*/
typedef enum _STATUS {
    CSdone, CSeof, CSmove, CSdispatch, CSstay
} STATUS;

typedef STATUS (*Keymap_Function)();


#include <stdio.h>
#if	defined(HAVE_STDLIB)
#include <stdlib.h>
#include <string.h>
#endif	/* defined(HAVE_STDLIB) */
#if	defined(SYS_UNIX)
#include "el_unix.h"
#endif	/* defined(SYS_UNIX) */
#if	defined(SYS_OS9)
#include "os9.h"
#endif	/* defined(SYS_OS9) */

#if	!defined(ESIZE_T)
#define ESIZE_T	unsigned int
#endif	/* !defined(ESIZE_T) */

typedef unsigned char	ECHAR;

#if	defined(HIDE)
#define STATIC	static
#else
#define STATIC	/* NULL */
#endif	/* !defined(HIDE) */

#if	!defined(CONST)
#if	defined(__STDC__)
#define CONST	const
#else
#define CONST
#endif	/* defined(__STDC__) */
#endif	/* !defined(CONST) */


#define MEM_INC		64
#define SCREEN_INC	256

#if 0
#define DISPOSE(p)	free((char *)(p))
#define NEW(T, c)	\
	((T *)malloc((unsigned int)(sizeof (T) * (c))))
#define RENEW(p, T, c)	\
	(p = (T *)realloc((char *)(p), (unsigned int)(sizeof (T) * (c))))
#define STRDUP(X) strdup(X)
#endif
#define COPYFROMTO(new, p, len)	\
	(void)memcpy((char *)(new), (char *)(p), (int)(len))
/* CSTR EST replacements -- awb */
#include "EST_walloc.h"
#define DISPOSE(p) wfree(p)
#define NEW(T,c) walloc(T,c)
#define RENEW(p,T,c) (p = wrealloc(p,T,c))
#define STRDUP(X) wstrdup(X)

/*
**  Variables and routines internal to this package.
*/
extern int	rl_eof;
extern int	rl_erase;
extern int	rl_intr;
extern int	rl_kill;
extern int	rl_quit;
extern int	el_user_intr;  /* with SIGINT if non-zero */
extern int      el_no_echo;    /* e.g under emacs, don't echo except prompt */
extern char	*rl_complete(char *pathname, int *unique);
extern int	rl_list_possib(char *pathname,char ***avp);
extern char *editline_history_file;
void rl_ttyset(int Reset);
void rl_add_slash(char *path,char *p);
int el_is_directory(char *path);
void do_user_intr();

#if	!defined(HAVE_STDLIB)
extern char	*getenv();
extern char	*malloc();
extern char	*realloc();
extern char	*memcpy();
extern char	*strcat();
extern char	*strchr();
extern char	*strrchr();
extern char	*strcpy();
extern char	*strdup();
extern int	strcmp();
extern int	strlen();
extern int	strncmp();

#endif	/* !defined(HAVE_STDLIB) */

/* Added prototypes for available functions in editline -- awb */
char * readline(CONST char* prompt);
void add_history(char *p);
void read_history(const char *history_file);
void write_history(const char *history_file);
typedef char **EL_USER_COMPLETION_FUNCTION_TYPE(char *text,int start, int end);
extern EL_USER_COMPLETION_FUNCTION_TYPE*el_user_completion_function;
char *el_current_sym();
void el_redisplay();
void el_bind_key_in_metamap(char c, Keymap_Function func);

