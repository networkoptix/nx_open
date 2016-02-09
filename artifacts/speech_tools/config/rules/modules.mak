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
 ## Amend the compilation options to reflect the selected modules.        ##
 ##                                                                       ##
 ###########################################################################

###########################################################################
## Override the usual module selection, used for testing.

ifneq ($(INCLUDE_EVERYTHING),)
ifeq ($(INCLUDE_EVERYTHING),ALL)
    INCLUDE_MODULES = $(ALL_MODULES)
    MODINCLUDES_DEPEND = config $(PROJECT_OTHER_CONFIGS)
    MODINCLUDES_FILE = modincludes_absolutely_everything.inc
else
    INCLUDE_MODULES += $(ALL_REAL_MODULES)
    MODINCLUDES_DEPEND = config $(PROJECT_OTHER_CONFIGS)
    MODINCLUDES_FILE = modincludes_everything.inc
endif
else
    MODINCLUDES_DEPEND = config $(PROJECT_OTHER_CONFIGS)
    MODINCLUDES_FILE = modincludes.inc
endif

###########################################################################
## The extra command line flags are divided into bunches so they can be
## used in just the necessary places. This is only for prettyness during
## compilation and the division is somewhat arbitrary.

## User interface things

UI_DEFINES =
UI_INCLUDES =

## Audio

AUDIO_DEFINES =
AUDIO_INCLUDES =

## To do with the core of festival

FESTIVAL_DEFINES =
FESTIVAL_INCLUDES =

## Stuff for the old diphone code.

MODULE_DIPHONE_DEFINES =

## Anything else -- gets included everywhere.

MODULE_DEFINES =
MODULE_INCLUDES =

## These are the extra libraries needed because of all these modules.

DEBUG_LIBS  =
MODULE_LIBS =

INCLUDE_MODULES += $(ALSO_INCLUDE)

ifneq ($(findstring NATIVE_AUDIO,$(INCLUDE_MODULES)),)
ifdef NATIVE_AUDIO_MODULE
    INCLUDE_MODULES += $(NATIVE_AUDIO_MODULE)_AUDIO
else
.config_error:: FORCE
	@echo "+--------------------------------------------------"
	@echo "| No Native Audio method for this type of system."
	@echo "+--------------------------------------------------"
	@exit 1
endif
endif

 ###########################################################################
 ##                                                                       ##
 ## Abandon hope... This next bit is shell and make magic to deal with    ##
 ## adding modules.                                                       ##
 ##                                                                       ##
 ###########################################################################


ifdef MODINCLUDES

$(MODINCLUDES_FILE) : $(MODINCLUDES_DEPEND)
	@echo Remake $(MODINCLUDES_FILE) >&2
	@TOP='$(TOP)' MODULE_DIRECTORY='$(MODULE_DIRECTORY)' \
		/bin/sh $(EST)/config/rules/modules.sh $(INCLUDE_MODULES) > $@
else

ifndef MADE_FROM_ABOVE


modules_dummy: $(shell $(MAKE) --no-print-directory -C $(TOP)/config  INCLUDE_EVERYTHING='$(INCLUDE_EVERYTHING)' MODINCLUDES=1 $(MODINCLUDES_FILE) >/dev/null )

$(EST)/config/$(MODINCLUDES_FILE) : 
	$(MAKE) --no-print-directory -C $(TOP)/config MADE_FROM_ABOVE=1  MODINCLUDES=1 INCLUDE_EVERYTHING='$(INCLUDE_EVERYTHING)' $(MODINCLUDES_FILE) 

endif

include $(TOP)/config/$(MODINCLUDES_FILE)

endif


