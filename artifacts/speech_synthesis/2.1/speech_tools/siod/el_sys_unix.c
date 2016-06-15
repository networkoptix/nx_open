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
**  Unix system-dependant routines for editline library.
*/
#include "editline.h"

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

extern CONST ECHAR el_NIL[];

int el_user_intr = 0;
int el_PushBack=0;
int el_Pushed=0;
CONST ECHAR	*el_Input = el_NIL;

extern void TTYflush();

#if	defined(HAVE_TCGETATTR)
#include <termios.h>

void rl_ttyset(int Reset)
{
    static struct termios	old;
    struct termios		new;

    if (Reset == 0) {
	(void)tcgetattr(0, &old);
	rl_erase = old.c_cc[VERASE];
	rl_kill = old.c_cc[VKILL];
	rl_eof = old.c_cc[VEOF];
	rl_intr = old.c_cc[VINTR];
	rl_quit = old.c_cc[VQUIT];

	new = old;
	new.c_cc[VINTR] = -1;
	new.c_cc[VQUIT] = -1;
	new.c_lflag &= ~(ECHO | ICANON);
	new.c_iflag &= ~(ISTRIP | INPCK);
	new.c_cc[VMIN] = 1;
	new.c_cc[VTIME] = 0;
#ifdef VDSUSP
         /* On Solaris (non-posix) DSUSP is ^Y, cancel it (awb 30/12/98) */
	new.c_cc[VDSUSP] = -1;
#endif
	    
	(void)tcsetattr(0, TCSANOW, &new);
    }
    else
	(void)tcsetattr(0, TCSANOW, &old);
}

#else
#include <sgtty.h>

void rl_ttyset(int Reset)
{
    static struct sgttyb	old_sgttyb;
    static struct tchars	old_tchars;
    struct sgttyb		new_sgttyb;
    struct tchars		new_tchars;

    if (Reset == 0) {
	(void)ioctl(0, TIOCGETP, &old_sgttyb);
	rl_erase = old_sgttyb.sg_erase;
	rl_kill = old_sgttyb.sg_kill;

	(void)ioctl(0, TIOCGETC, &old_tchars);
	rl_eof = old_tchars.t_eofc;
	rl_intr = old_tchars.t_intrc;
	rl_quit = old_tchars.t_quitc;

	new_sgttyb = old_sgttyb;
	new_sgttyb.sg_flags &= ~ECHO;
	new_sgttyb.sg_flags |= RAW;
#if	defined(PASS8)
	new_sgttyb.sg_flags |= PASS8;
#endif	/* defined(PASS8) */
	(void)ioctl(0, TIOCSETP, &new_sgttyb);

	new_tchars = old_tchars;
	new_tchars.t_intrc = -1;
	new_tchars.t_quitc = -1;
	(void)ioctl(0, TIOCSETC, &new_tchars);
    }
    else {
	(void)ioctl(0, TIOCSETP, &old_sgttyb);
	(void)ioctl(0, TIOCSETC, &old_tchars);
    }
}
#endif	/* defined(HAVE_TCGETATTR) */

unsigned int TTYget()
{
    ECHAR	c;
    int s;

    TTYflush();
    if (el_Pushed) {
	el_Pushed = 0;
	return el_PushBack;
    }
    if (*el_Input)
	return *el_Input++;
    s = read(0, &c, (ESIZE_T)1) == 1 ? c : EOF;
    return s;
}

void rl_add_slash(char *path,char *p)
{
    struct stat	Sb;

    if (stat(path, &Sb) >= 0)
	(void)strcat(p, S_ISDIR(Sb.st_mode) ? "/" : " ");
}

int el_is_directory(char *path)
{
    struct stat	Sb;

    if ((stat(path, &Sb) >= 0) && S_ISDIR(Sb.st_mode))
	return 1;
    else
	return 0;
}

void do_user_intr()
{
    if (el_user_intr)
	kill(getpid(),SIGINT);
}
