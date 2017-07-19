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
/*  this version of editline does not have the "no commercial use"          */
/*  restriction that some of the rest of the EST library may have           */
/*  awb Dec 30 1998                                                         */
/*                                                                          */
/****************************************************************************/
/*  $Revision: 1.2 $
**
**  Win32 system-dependant routines for editline library.
*/
#include <windows.h>
#include "editline.h"

extern CONST ECHAR el_NIL[];

int el_user_intr = 0;
int el_PushBack=0;
int el_Pushed=0;
CONST ECHAR	*el_Input = el_NIL;

extern void TTYflush();

STATIC HANDLE hStdin;

void rl_ttyset(int Reset)
{
  HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE); 
  hStdin = GetStdHandle(STD_INPUT_HANDLE); 

  SetConsoleMode(hStdin, 0);
  SetConsoleMode(hStdout, ENABLE_PROCESSED_OUTPUT);
}

unsigned int TTYget()
{
    ECHAR	c;
    int n;

    TTYflush();
    if (el_Pushed) {
	el_Pushed = 0;
	return el_PushBack;
    }
    if (*el_Input)
	return *el_Input++;
    if (!ReadFile(hStdin, &c, 1, &n, NULL))
      c= EOF;
    return c;
}

#if	!defined(S_ISDIR)
#define S_ISDIR(m)		(((m) & S_IFMT) == S_IFDIR)
#endif	/* !defined(S_ISDIR) */

void rl_add_slash(char *path,char *p)
{
#if 0 
  struct stat	Sb;

    if (stat(path, &Sb) >= 0)
	(void)strcat(p, S_ISDIR(Sb.st_mode) ? "\\" : " ");
#endif
}

int el_is_directory(char *path)
{

#if 0
    struct stat	Sb;

    if ((stat(path, &Sb) >= 0) && S_ISDIR(Sb.st_mode))
	return 1;
    else
#endif
      return 0;
}

void do_user_intr()
{
#if 0
    if (el_user_intr)
	kill(getpid(),SIGINT);
#endif
}

int tgetent(char *bp, const char *name)
{
  /* Always OK. */
  return 1;
}

int tgetnum(const char *id)
{
  if (strcmp(id, "co") == 0)
    return 80;
  else if (strcmp(id, "li") == 0)
    return 20;
  return 0;
}

#define ESC "\033"
#define ESCB "\033["


int tgetstr(const char *id, char **area)
{
  if (strcmp(id, "le") == 0)
    return (int)"\010";		/* BACKSPACE */
  else if (strcmp(id, "up") == 0)
    return 0; /* (int)ESCB "A"; */
  else if (strcmp(id, "cl") == 0)
    return (int)ESCB "H" ESCB "J";
  else if (strcmp(id, "nl") == 0)
    return (int)"\n";
  else if (strcmp(id, "cr") == 0)
    return (int)"\r";
  else if (strcmp(id, "nd") == 0)
    return 0;  /* (int)ESCB "C"; */
  return 0;
}
