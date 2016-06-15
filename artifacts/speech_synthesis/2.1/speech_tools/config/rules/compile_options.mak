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
 ##                                                                       ##
 ## Extend the compiler options depending on the behaviour requested.     ##
 ##                                                                       ##
 ###########################################################################

###########################################################################
## Look for directory specific options


DIRNAME_AS_FILE := $(subst /,_,$(DIRNAME))

ifneq ($(origin OPTIMISE_$(DIRNAME_AS_FILE)),undefined)
	OPTIMISE := $(OPTIMISE_$(DIRNAME_AS_FILE))
endif

ifneq ($(origin WARN_$(DIRNAME_AS_FILE)),undefined)
	WARN := $(WARN_$(DIRNAME_AS_FILE))
endif

ifneq ($(origin VERBOSE_$(DIRNAME_AS_FILE)),undefined)
	VERBOSE := $(VERBOSE_$(DIRNAME_AS_FILE))
endif

ifneq ($(origin DEBUG_$(DIRNAME_AS_FILE)),undefined)
	DEBUG := $(DEBUG_$(DIRNAME_AS_FILE))
endif

ifneq ($(origin PROFILE_$(DIRNAME_AS_FILE)),undefined)
	PROFILE := $(PROFILE_$(DIRNAME_AS_FILE))
endif

ifneq ($(origin SHARED_$(DIRNAME_AS_FILE)),undefined)
	SHARED := $(SHARED_$(DIRNAME_AS_FILE))
endif

ifneq ($(origin STATIC_$(DIRNAME_AS_FILE)),undefined)
	STATIC := $(STATIC_$(DIRNAME_AS_FILE))
endif

###########################################################################
## Normalise

ifndef WARN
	WARN = 0
endif

ifndef DEBUG
	DEBUG = 0
endif

ifndef PROFILE
	PROFILE = 0
endif

ifndef OPTIMISE
	OPTIMISE = 0
endif

ifndef SHARED
	SHARED = 0
endif

ifndef STATIC
	STATIC = 0
endif

ifndef VERBOSE
	VERBOSE = 0
endif



###########################################################################
## Now set the compile options as requested

ifneq ($(DEBUG),0)
ifneq ($(OPTIMISE),4)
    CFLAGS    += $(DEBUG_CCFLAGS)
    CXXFLAGS  += $(DEBUG_CXXFLAGS)
    JAVAFLAGS += $(DEBUG_JAVAFLAGS)
    JAVAC      := $(DEBUG_JAVAC)
    JAVAH      := $(DEBUG_JAVAH)
    LINKFLAGS += $(DEBUG_LINKFLAGS)
endif
endif

ifneq ($(PROFILE),0)
ifndef PROFILE_$(PROFILE)_CCFLAGS
    PROFILE := $(PROFILE_DEFAULT)
endif
    CFLAGS    += $(PROFILE_$(PROFILE)_CCFLAGS)
    CXXFLAGS  += $(PROFILE_$(PROFILE)_CXXFLAGS)
    LINKFLAGS += $(PROFILE_$(PROFILE)_LINKFLAGS)
endif

ifneq ($(OPTIMISE),0)
    CFLAGS    += $(OPTIMISE_CCFLAGS)
    CXXFLAGS  += $(OPTIMISE_CXXFLAGS)
    JAVAFLAGS += $(OPTIMISE_JAVAFLAGS)
    LINKFLAGS += $(OPTIMISE_LINKFLAGS)
endif

ifneq ($(SHARED),0)
    CFLAGS    += $(SHARED_CCFLAGS)
    CXXFLAGS  += $(SHARED_CXXFLAGS)
    LINKFLAGS += $(SHARED_LINKFLAGS)
    CXXDLFLAGS = 
else
    CXXDLFLAGS   += $(SHARED_CXXFLAGS)
endif


ifneq ($(WARN),0)
    CFLAGS    += $(WARN_CCFLAGS)
    CXXFLAGS  += $(WARN_CXXFLAGS)
    JAVAFLAGS += $(WARN_JAVAFLAGS)
    LINKFLAGS += $(WARN_LINKFLAGS)
else
    CFLAGS    += $(NOWARN_CCFLAGS)
    CXXFLAGS  += $(NOWARN_CXXFLAGS)
    JAVAFLAGS += $(NOWARN_JAVAFLAGS)
    LINKFLAGS += $(NOWARN_LINKFLAGS)
endif


ifneq ($(VERBOSE),0)
    CFLAGS    += $(VERBOSE_CCFLAGS)
    CXXFLAGS  += $(VERBOSE_CXXFLAGS)
    JAVAFLAGS += $(VERBOSE_JAVAFLAGS)
    LINKFLAGS += $(VERBOSE_LINKFLAGS)
endif

ifneq ($(STATIC),0)
    CFLAGS    += $(STATIC_CCFLAGS)
    CXXFLAGS  += $(STATIC_CXXFLAGS)
    LINKFLAGS += $(STATIC_LINKFLAGS)
endif

