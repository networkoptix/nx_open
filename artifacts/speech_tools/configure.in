dnl########################################################################
dnl                                                                      ##
dnl               Centre for Speech Technology Research                  ##
dnl                    University of Edinburgh, UK                       ##
dnl                      Copyright (c) 1996-2001                         ##
dnl                       All Rights Reserved.                           ##
dnl                                                                      ##
dnl Permission is hereby granted, free of charge, to use and distribute  ##
dnl this software and its documentation without restriction, including   ##
dnl without limitation the rights to use, copy, modify, merge, publish,  ##
dnl distribute, sublicense, and/or sell copies of this work, and to      ##
dnl permit persons to whom this work is furnished to do so, subject to   ##
dnl the following conditions:                                            ##
dnl  1. The code must retain the above copyright notice, this list of    ##
dnl     conditions and the following disclaimer.                         ##
dnl  2. Any modifications must be clearly marked as such.                ##
dnl  3. Original authors' names are not deleted.                         ##
dnl  4. The authors' names are not used to endorse or promote products   ##
dnl     derived from this software without specific prior written        ##
dnl     permission.                                                      ##
dnl                                                                      ##
dnl THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        ##
dnl DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ##
dnl ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   ##
dnl SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     ##
dnl FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    ##
dnl WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   ##
dnl AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          ##
dnl ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       ##
dnl THIS SOFTWARE.                                                       ##
dnl                                                                      ##
dnl########################################################################
AC_INIT(include/EST.h)

AC_CANONICAL_SYSTEM
AC_PROG_CC
if test "x$GCC" = "xyes"; then
	CFLAGS="$CFLAGS -Wall"
fi
AC_PROG_RANLIB
AC_CHECK_TOOL(AR, ar)

AC_C_BIGENDIAN

AC_CHECK_LIB(ncurses, tputs, [TERMCAPLIB=-lncurses], [TERMCAPLIB=-lcurses])
dnl if test "$TERMCAPLIB" != "-ltermcap"; then
dnl    AC_CHECK_LIB(termcap, tputs, [TERMCAPLIB=-ltermcap], [TERMCAPLIB=-lncurses])
dnl 
AC_SUBST(TERMCAPLIB)

dnl Which version of GCC do we have here
COMPILERTYPE=gcc
AC_TRY_COMPILE([#include <stdio.h>],
               [int j= 
                #if __GNUC__ == 2 && __GNUC_MINOR__ == 95
                3;
                #endif ],
               [COMPILERTYPE=gcc295])
AC_TRY_COMPILE([#include <stdio.h>],
               [int j= 
                #if __GNUC__ == 3 
                3;
                #endif ],
               [COMPILERTYPE=gcc32])
AC_TRY_COMPILE([#include <stdio.h>],
               [ int j=
                #if __GNUC__ == 2 && __GNUC_MINOR__ == 96
                3;
                #endif ],
               [COMPILERTYPE=gcc296])
AC_TRY_COMPILE([#include <stdio.h>],
               [int j=
                #if __GNUC__ == 2 && __GNUC_MINOR__ == 91
                3;
                #endif ],
               [COMPILERTYPE=egcs])
AC_TRY_COMPILE([#include <stdio.h>],
               [int j=
                #if __GNUC__ == 2 && __GNUC_MINOR__ == 8
                3;
                #endif ],
               [COMPILERTYPE=gcc28])
AC_SUBST(COMPILERTYPE)

LINUXAUDIO=none
AC_TRY_COMPILE([#include <alsa/version.h>],
               [int j=
                #if SND_LIB_SUBMINOR >= 11
                3;
                #endif],
              [LINUXAUDIO="alsa"])
AC_SUBST(LINUXAUDIO)

AC_OUTPUT(config/config)
