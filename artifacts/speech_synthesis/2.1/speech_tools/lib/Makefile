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
#         library directory
#                                                           
#========================================================================##
TOP=..
DIRNAME=lib

SKELETONS = est_program_makefile-in est_mainline-in

FILES = Makefile cstrutt.dtd $(SKELETONS) VCLocalRules
BUILT_LIBRARIES = $(PROJECT_LIBRARIES:%=lib%.a)
SHARED_LIBRARIES = $(PROJECT_SHARED_LIBRARIES:%=lib%.so*)

ALL_DIRS = example_data siod 
LOCAL_CLEAN = $(BUILT_LIBRARIES) $(SHARED_LIBRARIES) est.jar $(SKELETONS:%-in=%)

INSTALL = libfile

ALL = $(SKELETONS:%-in=%) .libraries 

include $(TOP)/config/common_make_rules

libfile:
	$(INSTALL_PROG) libestools.a libeststring.a $(LIBDIR)/

CXX_COMMAND_ABS=$(subst $(TOP),$$(EST_HOME),$(CXX_COMMAND))
CC_COMMAND_ABS=$(subst $(TOP),$$(EST_HOME),$(CXX_COMMAND))
LINK_COMMAND_ABS=$(subst $(TOP),$$(EST_HOME),$(LINK_COMMAND))
INCLUDES_ABS=$(subst $(TOP),$$(EST_HOME),$(INCLUDES))
LIBS_ABS=$(subst $(TOP),$$(EST_HOME),$(LIBS))

skeletons: $(SKELETONS:%-in=%)

$(SKELETONS:%-in=%): $(TOP)/config/config

$(SKELETONS:%-in=%) : %: %-in
	@echo make $* from $*-in
	@- if [ -f $* ] ; then  chmod +w $* ; fi
	@$(NAWK) -f  "$(EST)/config/rules/script_process.awk" \
		project='$(PROJECT_NAME)' \
		version='$(PROJECT_VERSION)' \
		systemtype='$(MACHINETYPE)_$(OSTYPE)$(OSREV)' \
		topdir='$($(PROJECT_PREFIX)_HOME)' \
		lib='$($(PROJECT_PREFIX)_HOME)/$(PROJECT_LIBRARY_DIR)' \
		includes='$(INCLUDES_ABS)' \
		defines='$(DEFINES)' \
		libs='$(LIBS_ABS)' \
		est='$(EST_HOME)' \
		classpath='$(SYSTEM_JAVA_CLASSPATH)' \
		perl='$(PERL)' \
		javahome='$(JAVA_HOME)' \
		java='$(JAVA)' \
		javac='$(JAVAC)' \
		ldpath='$(SYSTEM_LD_LIBRARY_PATH)' \
		cccommand='$(CC_COMMAND_ABS)' \
		cxxcommand='$(CXX_COMMAND_ABS)' \
		linkcommand='$(LINK_COMMAND_ABS)' \
			$*-in > $*
	@-chmod -w $*










