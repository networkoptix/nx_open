###########################################################################
##                                                                       ##
##                Centre for Speech Technology Research                  ##
##                     University of Edinburgh, UK                       ##
##                       Copyright (c) 1996-2014                         ##
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
##     Top level Makefile for Edinburgh Speech tools library             ##
##     Authors: Paul Taylor, Simon King, Alan W Black, Richard Caley     ##
##                 and others (see ACKNOWLEDGEMENTS)                     ##
##     Version: 2.4 release December 2014                                ##
##                                                                       ##
###########################################################################

TOP=.
DIRNAME=.
LIB_BUILD_DIRS = audio utils base_class ling_class speech_class sigpr \
             stats grammar intonation
# Development directories are those that contain non-stable code
#DEV_DIRS = $(shell if [ -f DEV_DIRS ]; then cat DEV_DIRS; fi )
BUILD_DIRS = $(LIB_BUILD_DIRS) lib main scripts testsuite bin
TEMPLATE_DIRS=include audio utils base_class base_class/string \
              ling_class speech_class sigpr stats grammar siod
EXTRA_DIRS=siod java rxp wrappers
ALL_DIRS = include $(BUILD_DIRS) $(EXTRA_DIRS) config doc 
VERSION=$(PROJECT_VERSION)
CONFIG=configure configure.in config.sub config.guess \
       missing install-sh mkinstalldirs
FILES=Makefile README INSTALL $(CONFIG)

LOCAL_CLEAN= Build.trace Test.trace Templates.DB

ALL = .config_error .sub_directories

# Try and say if config hasn't been created
config_dummy := $(shell test -f config/config || ( echo '*** '; echo '*** Making default config file ***'; echo '*** '; ./configure; )  >&2)

# force a check on the system file
system_dummy := $(shell $(MAKE) -C $(TOP)/config -f make_system.mak TOP=.. system.mak)

include $(TOP)/config/common_make_rules

dist:	backup
backup:  time-stamp
	 @ $(MAKE) file-list
	 @ sed 's/^\.\///' <FileList | sed 's/^/speech_tools\//' >FileList.all
	 @ (cd ..; tar cvf - `cat speech_tools/FileList.all` speech_tools/.time-stamp | gzip > speech_tools/speech_tools-$(VERSION)-$(PROJECT_STATE).tar.gz)
	 @ $(RM) -f $(TOP)/FileList.all
	 @ ls -l speech_tools-$(VERSION)-$(PROJECT_STATE).tar.gz

time-stamp :
	@ echo speech_tools $(VERSION) >.time-stamp
	@ date >>.time-stamp

minrev:  backup
	@mv speech_tools-$(VERSION).tar.gz speech_tools-$(VERSION).`date +%y%m%d`.tar.gz

tags:
	@ $(RM) -f FileList
	@ $(MAKE) --no-print-directory FileList
	@ etags `grep ".[ch]c*$$" FileList`

test: make_library
	@ $(MAKE) --no-print-directory -C testsuite test

rebuild_and_test:
	$(MAKE) -k clean
	$(MAKE) -k depend
	$(MAKE) -k all >Build.trace 2>&1 
	$(MAKE) -k test >Test.trace 2>&1 
	@if egrep 'FAILED|INCORRECT' Test.trace ;\
		then \
		echo test failed ;\
		exit 1 ;\
	fi
	@if cat Build.trace Test.trace | egrep 'warning:' ;\
		then \
		echo warnings found ;\
		exit 2 ;\
	fi

config/config: config/config.in config.status
	./config.status

configure: configure.in
	autoconf

include $(TOP)/config/rules/top_level.mak
include $(TOP)/config/rules/install.mak
