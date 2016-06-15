 ###########################################################################
 ##                                                                       ##
 ##                Centre for Speech Technology Research                  ##
 ##                     University of Edinburgh, UK                       ##
 ##                         Copyright (c) 1999                            ##
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
 ##                 Author: Rob Clark                                     ##
 ##                   Date: Feb 2004                                      ##
 ## --------------------------------------------------------------------  ##
 ## Settings for intec cc 8.0                                             ##
 ##                                                                       ##
 ###########################################################################

CC=icc
CXX=icpc

COMPILER_DESC=Intel 8.0
COMPILER_VERSION_COMMAND=$(CXX) -v 2>&1 | tail -1 | sed -e 's/^Version.//'

CFLAGS  =  $(CC_OTHER_FLAGS)
CXXFLAGS  =  $(CC_OTHER_FLAGS)

DEBUG_CCFLAGS   = -g
DEBUG_CXXFLAGS  = -g
DEBUG_LINKFLAGS = -g

WARN_CCFLAGS   = -Wall
WARN_CXXFLAGS  = -Wall
WARN_LINKFLAGS = -Wall

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

SHARED_CCFLAGS  = -fpic 
SHARED_CXXFLAGS  = -fPIC
SHARED_LINKFLAGS = 

MAKE_SHARED_LIB = $(CXX) -shared -o XXX

#-shared -R$(MAIN_LIBRARY_DIR)

STATIC_CCFLAGS   = 
STATIC_CXXFLAGS  = 
STATIC_LINKFLAGS = -Dstatic

TEMPLATE_SPECIFIC = -DINSTANTIATE_TEMPLATES
TEMPLATE_ARGS = 

BUILD_LIB   = $(AR) cruv
INDEX_LIB   = $(RANLIB)

WARN_CXXFLAGS  +=  


MAKE_DEPEND_C = $(CC) -MM $(INCLUDES) $(TEMPLATES) $(TEMPLATE_SPECIFIC)
MAKE_DEPEND_CXX = $(CC) -MM $(INCLUDES) $(WARN_CXXFLAGS) $(TEMPLATES) $(TEMPLATE_SPECIFIC)

COMPILERLIBS= $(COMPILER_LIBS_DIR:%=-L%) 




