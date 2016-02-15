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
 ## Settings for GCC.                                                     ##
 ##                                                                       ##
 ###########################################################################

CC=gcc
CXX=gcc
ifeq ($(OSTYPE),Darwin)
CXX=g++
endif

COMPILER_DESC=FSF gcc
COMPILER_VERSION_COMMAND=$(CXX) -v 2>&1 | tail -1 | sed -e 's/^....//'

CFLAGS  = $(GCC_SYSTEM_OPTIONS) $(CC_OTHER_FLAGS)
CXXFLAGS  =  $(GCC_SYSTEM_OPTIONS) -fno-implicit-templates $(CC_OTHER_FLAGS)

DEBUG_CCFLAGS   = -g
DEBUG_CXXFLAGS  = -g
DEBUG_LINKFLAGS = -g

WARN_CCFLAGS   = -Wall
WARN_CXXFLAGS  = -Wall
WARN_LINKFLAGS = -Wall

VERBOSE_CCFLAGS   =
VERBOSE_CXXFLAGS  = 
VERBOSE_LINKFLAGS =

OPTIMISE_CCFLAGS   = -O$(OPTIMISE)
OPTIMISE_CXXFLAGS  = -O$(OPTIMISE)
OPTIMISE_LINKFLAGS = -O$(OPTIMISE)

PROFILE_DEFAULT = gprof

PROFILE_prof_CCFLAGS   = -p
PROFILE_prof_CXXFLAGS  = -p
PROFILE_prof_LINKFLAGS = -p

PROFILE_gprof_CCFLAGS   = -pg
PROFILE_gprof_CXXFLAGS  = -pg
PROFILE_gprof_LINKFLAGS = -pg

SHARED_CCFLAGS  = -fPIC 
SHARED_CXXFLAGS  = -fPIC 
SHARED_LINKFLAGS = 

ifndef GCC_MAKE_SHARED_LIB
    MAKE_SHARED_LIB = $(CXX) -shared -fno-shared-data -o XXX
else
    MAKE_SHARED_LIB = $(GCC_MAKE_SHARED_LIB)
endif

#-shared -R$(MAIN_LIBRARY_DIR)

STATIC_CCFLAGS   = 
STATIC_CXXFLAGS  = 
STATIC_LINKFLAGS = -static

TEMPLATE_SPECIFIC = -DINSTANTIATE_TEMPLATES
TEMPLATE_ARGS = 

## The -lgcc here is redundant - gcc does this anyway - but it
## helps java know what needs to be loaded.

COMPILERLIBS= $(COMPILER_LIBS_DIR:%=-L%) -lstdc++

## special ways of doing things, blank means default

MAKE_DEPEND_C = $(CC) -MM $(INCLUDES) $(TEMPLATES) $(TEMPLATE_SPECIFIC)
MAKE_DEPEND_CXX = $(CC) -MM $(INCLUDES) $(TEMPLATES) $(TEMPLATE_SPECIFIC)
BUILD_LIB   = $(AR) cruv
INDEX_LIB   = $(RANLIB)



