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
 ##                   Date: Wed Oct  1 1997                               ##
 ## --------------------------------------------------------------------  ##
 ## Default values for the things which system, compiler and local        ##
 ## configurations can reset.                                             ##
 ##                                                                       ##
 ###########################################################################

###########################################################################
## Some things we often want to do

ifndef ENSURE_EMPTY_DIR
    ENSURE_EMPTY_DIR=if [ ! -d 'DIR' ] ; then mkdir 'DIR' ; else $(RM) -rf 'DIR'/* ; fi
endif

ifndef ENSURE_DIR
    ENSURE_DIR=if [ ! -d 'DIR' ] ; then mkdir 'DIR' ; fi
endif

###########################################################################
## default names for make related files

ifndef DEPEND
    DEPEND=make.depend
endif

ifndef MAKE_INCLUDE
    MAKE_INCLUDE=make.include
endif

ifndef INLIB
    INLIB = $(MAIN_LIBRARY)
endif

ifdef N
	MADE_FROM_ABOVE:=$(N)
endif

ifndef PROJECT_LIBDEPS
    PROJECT_LIBDEPS = $(foreach l,$(PROJECT_LIBRARIES),$(PROJECT_LIBRARY_DIR_$(l))/lib$(l).a)
endif
ifndef PROJECT_LIBS
    PROJECT_LIBS = $(foreach l,$(PROJECT_LIBRARIES),-L$(PROJECT_LIBRARY_DIR_$(l)) -l$(l))
endif

ifndef REQUIRED_LIBDEPS
    REQUIRED_LIBDEPS = $(foreach l,$(REQUIRED_LIBRARIES),$(REQUIRED_LIBRARY_DIR_$(l))/lib$(l).a)
endif
ifndef REQUIRED_LIBS
    REQUIRED_LIBS = $(foreach l,$(REQUIRED_LIBRARIES),-L$(REQUIRED_LIBRARY_DIR_$(l)) -l$(l))
endif

ifndef LIBRARIES
    LIBRARIES = $(PROJECT_LIBRARIES) $(LOCAL_LIBRARIES)
endif

ifndef LIBDEPS
    LIBDEPS = $(PROJECT_LIBDEPS) $(REQUIRED_LIBDEPS) $(LOCAL_LIBDEPS)
endif

ifndef LOCAL_DEFAULT_LIBRARY
    LOCAL_DEFAULT_LIBRARY = $(PROJECT_DEFAULT_LIBRARY)
endif

## if not set the main lirary gets all object files

ifndef OBJS_$(LOCAL_DEFAULT_LIBRARY)
    OBJS_$(LOCAL_DEFAULT_LIBRARY) := $(OBJS)
endif

## If we haven't given an explicit list, gather together all
## the object files

ifndef OBJS
    OBJS := $(foreach lib,$(LIBRARIES),$(OBJS_$(lib)))
endif

###########################################################################
## Collections of arguments

## All the defines for a normal file to be compiled

ifndef DEFINES
    DEFINES = $(CONFIG_DEFINES) $(DEBUG_DEFINES) $(LOCAL_DEFINES) $(MODULE_DEFINES) $(PROJECT_DEFINES) $(OS_DEFINES)
endif

## All the includes for a normal file to be compiled

ifndef INCLUDES
    INCLUDES = $(CONFIG_INCLUDES) $(LOCAL_INCLUDES) $(MODULE_INCLUDES) $(PROJECT_INCLUDES) $(OS_INCLUDES)
endif

## Places to  look for templates.

# ifndef TEMPLATE_DIRS
    TEMPLATE_DIRS = $(CONFIG_TEMPLATE_DIRS) $(LOCAL_TEMPLATE_DIRS) $(MODULE_TEMPLATE_DIRS) $(PROJECT_TEMPLATE_DIRS:%=$(TOP)/%)  $(foreach library,$(REQUIRED_LIBRARIES),$(LIBRARY_TEMPLATE_DIRS_$(library))) $(OS_TEMPLATE_DIRS)
# endif

ifndef TEMPLATES
    TEMPLATES= $(CONFIG_TEMPLATES) $(LOCAL_TEMPLATES) $(TEMPLATE_ARGS)
endif

ifndef DEPEND_FLAGS
    DEPEND_FLAGS=$(DEFINES)
endif

## Libraries to link a major program of this project

ifndef MATH_LIBRARY
    MATH_LIBRARY= -lm
endif

ifndef LIBS
    LIBS = $(CONFIG_LIBS) $(LOCAL_LIBS) $(PROJECT_LIBS) $(REQUIRED_LIBS) $(MODULE_LIBS) $(MODULE_EXTRA_LIBS) $(DEBUG_LIBS) $(OS_LIBS) $(MATH_LIBRARY) $(COMPILERLIBS) 
endif

## Libraries to link utility programs -- doesn't use this project's libraries

ifndef NON_PROJECT_LIBS
     NON_PROJECT_LIBS = $(CONFIG_LIBS) $(LOCAL_LIBS) $(REQUIRED_LIBS) \
	$(SYSTEM_LD_LIBRARY_PATH:%=-L%) \
	$(MODULE_LIBS) $(MODULE_EXTRA_LIBS) $(DEBUG_LIBS) $(OS_LIBS) $(MATH_LIBRARY) $(COMPILERLIBS) 

# reduced list for linking with java code.

     JAVA_PROJECT_LIBS = $(CONFIG_LIBS) $(LOCAL_LIBS) $(REQUIRED_LIBS) \
	$(SYSTEM_LD_LIBRARY_PATH:%=-L%) \
	$(MODULE_LIBS) $(DEBUG_LIBS) $(OS_LIBS) $(MATH_LIBRARY) $(COMPILERLIBS) 
endif

## Default C Compilation

ifndef	CC_COMMAND
    CC_COMMAND = $(CC) -c $(CFLAGS) $(COMPILE_CCFLAGS) $(DEFINES) $(INCLUDES)
endif

## Default C++ Compilation

ifndef CXX_COMMAND
    CXX_COMMAND = $(CXX) -c $(CXXFLAGS) $(COMPILE_CXXFLAGS) $(DEFINES) $(INCLUDES) $(TEMPLATES) 
endif

## C++ for dynamic loading

ifndef CXX_COMMAND_DL
    CXX_COMMAND_DL = $(CXX) -c $(CXXFLAGS) $(CXXDLFLAGS) $(COMPILE_CXXFLAGS) $(DEFINES) $(INCLUDES) $(TEMPLATES) 
endif

ifndef CXX_COMMAND_NOOPT
ifdef HONOUR_NOOPT
    CXX_COMMAND_NOOPT = $(subst $(OPTIMISE_CXXFLAGS),,$(CXX) -c $(CXXFLAGS)  $(COMPILE_CXXFLAGS) $(DEFINES) $(INCLUDES) $(TEMPLATES))
else
    CXX_COMMAND_NOOPT = $(CXX_COMMAND)
endif
endif

## C++ with template instantiations

ifndef CXX_COMMAND_TEMPLATES
    CXX_COMMAND_TEMPLATES = $(CXX_COMMAND) $(TEMPLATE_SPECIFIC)
endif

## Link a program (not including libraries)

ifndef LINK_COMMAND
    LINK_COMMAND = $(CXX) $(LINKFLAGS) $(TEMPLATES) 
endif

