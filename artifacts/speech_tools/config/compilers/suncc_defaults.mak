 ###########################################################################
 ##                                                                       ##
 ##                Centre for Speech Technology Research                  ##
 ##                     University of Edinburgh, UK                       ##
 ##                         Copyright (c) 1996                            ##
 ##                        All Rights Reserved.                           ##
 ##                                                                       ##
 ##  Permission is hereby granted, free of charge, to use and distribute  ##
 ##  this software and its documentation without restriction, including   ##
 ##  without limitation the rights to use, copy, modify, merge, publish,  ##
 ##  distribute, sublicense, and/or sell copies of this work, and to      ##
 ##  permit persons to whom this work is furnished to do so, subject to   ##
 ##  the following conditions:                                            ##
 ##   1. The code must retain the above copyright notice, this list of    ##
 ##      conditions and the following disclaimer.                         ##
 ##   2. Any modifications must be clearly marked as such.                ##
 ##   3. Original authors' names are not deleted.                         ##
 ##   4. The authors' names are not used to endorse or promote products   ##
 ##      derived from this software without specific prior written        ##
 ##      permission.                                                      ##
 ##                                                                       ##
 ##  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        ##
 ##  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ##
 ##  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   ##
 ##  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     ##
 ##  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    ##
 ##  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   ##
 ##  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          ##
 ##  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       ##
 ##  THIS SOFTWARE.                                                       ##
 ##                                                                       ##
 ###########################################################################
 ##                                                                       ##
 ##                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             ##
 ##                   Date: Thu Oct  2 1997                               ##
 ## --------------------------------------------------------------------  ##
 ## Settings for unbundled sun compiler.                                  ##
 ##                                                                       ##
 ###########################################################################

CC = cc
CXX = CC 

COMPILER_DESC=Sun CC
COMPILER_VERSION_COMMAND=$(CXX) -V 2>&1 | sed -e '/CC: /{s///;q;}'

CFLAGS =$(suncc_system_options) $(CC_OTHER_FLAGS)
CXXFLAGS = $(suncc_system_options) -D__svr4__ $(CC_OTHER_FLAGS)

DEBUG_CCFLAGS  = -g
DEBUG_CXXFLAGS =  -g
DEBUG_LINKFLAGS =  -g

WARN_CCFLAGS  = +w +w2
WARN_CXXFLAGS = +w +w2
WARN_LINKFLAGS = +w +w2

VERBOSE_CCFLAGS  =
VERBOSE_CXXFLAGS = -ptv
VERBOSE_LINKFLAGS = -ptv

OPTIMISE_CCFLAGS  = -xO$(OPTIMISE)
OPTIMISE_CXXFLAGS = -O$(OPTIMISE)
OPTIMISE_LINKFLAGS = -O$(OPTIMISE)

PROFILE_DEFAULT = gprof

PROFILE_prof_CCFLAGS   = -p
PROFILE_prof_CXXFLAGS  = -p
PROFILE_prof_LINKFLAGS = -p

PROFILE_gprof_CCFLAGS   = -pg
PROFILE_gprof_CXXFLAGS  = -pg
PROFILE_gprof_LINKFLAGS = -pg

SHARED_CCFLAGS  = -KPIC
SHARED_CXXFLAGS = -KPIC

ifndef SUNCC_MAKE_SHARED_LIB
    MAKE_SHARED_LIB = $(CXX) $(TEMPLATES) -G -o XXX
else
    MAKE_SHARED_LIB = $(SUNCC_MAKE_SHARED_LIB)
endif

TEMPLATE_SPECIFIC =
TEMPLATE_ARGS = $(TEMPLATE_DIRS:%=-pti%) $(PROJECT_TEMPLATE_DBS:%=-ptr%) 

## special ways of doing things, blank means default

MAKE_DEPEND_C = $(CC) $(INCLUDES) $(TEMPLATES) -xM1
MAKE_DEPEND_CXX = $(CXX) $(INCLUDES) $(TEMPLATES) -xM1
BUILD_LIB   = $(CXX) $(TEMPLATES) -xar -o
INDEX_LIB   = $(DO_NOTHING_ARGS)

COMPILERLIBS=


