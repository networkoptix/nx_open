 ########################################################-*-mode:Makefile-*-
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
 ##                 Author: Various People                                ##
 ##                       : Reorganised (and probably broken)             ##
 ##                       : by Richard Caley (rjc@cstr.ed.ac.uk)          ##
 ##                   Date: June 1997                                     ##
 ## --------------------------------------------------------------------- ##
 ## Default Makefile rules includede everywhere.                          ##
 ##                                                                       ##
 ###########################################################################


# Map selected modules to how to include them.
include $(EST)/config/rules/modules.mak

ifdef REQUIRED_MAKE_INCLUDE
-include $(REQUIRED_MAKE_INCLUDE)
endif

-include $(TOP)/config/project_config_check.mak

# Places to find templates, needed by Sun CC
# export TEMPLATE_DIRS
export CI_ARGS

ifdef JAVA_CLASSES
	NEED_JAVA=1
endif

# Various subsets of directories

ifndef ALL_DIRS
ifdef BUILD_DIRS
    ALL_DIRS = $(BUILD_DIRS)
endif
endif

ifdef JAVA_COMMON_DIRS
    JUST_BUILD_DIRS := $(JUST_BUILD_DIRS) $(notdir $(JAVA_COMMON_DIRS))
endif

ifdef JUST_LIB
    SUBDIRECTORIES = ${JUST_BUILD_DIRS} ${LIB_BUILD_DIRS} ${EXTRA_LIB_BUILD_DIRS}  
else
    SUBDIRECTORIES =  ${JUST_BUILD_DIRS} ${EXTRA_LIB_BUILD_DIRS} ${BUILD_DIRS} ${EXTRA_BUILD_DIRS} 
endif


# now include various rule-sets

include $(EST)/config/rules/defaults.mak

include $(EST)/config/rules/compile_options.mak

include $(EST)/config/rules/make_depend.mak

include $(EST)/config/rules/library.mak

include $(EST)/config/rules/targets.mak

include $(EST)/config/rules/visual_c.mak

include $(EST)/config/rules/cvs.mak

include $(EST)/config/rules/c.mak

ifdef NEED_JAVA
include $(EST)/config/rules/java.mak
endif

include $(EST)/config/rules/config_errors.mak

# keep track of the places we put templates. Used for compiling things
# which depend on this

ifndef MADE_FROM_ABOVE
all: $(MAKE_INCLUDE)
$(MAKE_INCLUDE) : FORCE
	@echo "LIBRARY_TEMPLATE_DIRS=$(TEMPLATE_DIRS)" > $(MAKE_INCLUDE)
endif

# Finally, here are the automatically updated dependencies
ifndef NO_DEPEND
ifdef SRCS
-include $(DEPEND)
endif
endif

echo:
	@echo $(VAR)='$($(VAR))'

