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
 ## Main make targets.                                                    ##
 ##                                                                       ##
 ###########################################################################

###########################################################################
## utility targets

nothing:
	@$(DO_NOTHING)

FORCE:

###########################################################################
## Main make rule for building

.sub_directories: $(SUBDIRECTORIES)

${SUBDIRECTORIES} dummy_dir_name: FORCE
	@if [ ! -f $@/Makefile ] ;\
		then \
		echo "Makeing new directory $(DIRNAME)/$@" ;\
		mkdir $@ ;\
		: $(subst XXX,$@,$(MAKE_RCSED_DIR)) ;\
		$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $@ update ; \
	fi
	@ echo "Making in directory $(DIRNAME)/$@ ..."
	@ ${MAKE}  --no-print-directory -C $@ MADE_FROM_ABOVE=yes

###########################################################################
## Clean up junk

clean:
	$(RM) -fr $(OBJS) $(JAVA_CLASSES_CLASS) $(ALL_EXECS) $(ALL_EXECS:%=%.mak) $(ALL_LIBS) $(LOCAL_CLEAN) make.depend .buildlib* *~
ifdef ALL_DIRS
	@ for i in $(ALL_DIRS) ; \
	do \
	   echo "clean in $(DIRNAME)/$$i" ;\
	   $(MAKE) --no-print-directory -C $$i clean ; \
	done
endif
ifdef EXTRA_LIB_BUILD_DIRS
	@ for i in $(EXTRA_LIB_BUILD_DIRS) ; \
	do \
	   echo "clean in $(DIRNAME)/$$i" ;\
	   $(MAKE) --no-print-directory -C $$i clean ; \
	done
endif

###########################################################################
## strip executables

strip: FORCE
	@echo "strip in $(DIRNAME)"
ifdef ALL_EXECS
	@ for i in $(ALL_EXECS) ; \
	do  \
	   $(STRIP) $$i ; \
	done
else
	@ echo > /dev/null
endif
ifdef ALL_DIRS
	@ for i in $(ALL_DIRS) ; \
	do \
	   $(MAKE) --no-print-directory -C $$i strip ; \
	done
endif

###########################################################################
## Force remake of dependencies

ifndef DEPEND_DIRS
DEPEND_DIRS:=$(ALL_DIRS)
endif

depend: FORCE
	@echo "depend in $(DIRNAME)"
	@ $(RM) -f $(DEPEND)
	@ # a little hack to get it to remake make.depend
	@ $(MAKE)  --no-print-directory nothing
ifeq ($(DEPEND_DIRS),)
else
	@ for i in $(DEPEND_DIRS) ; \
	do \
	   $(MAKE) --no-print-directory -C $$i depend ; \
	done
endif

###########################################################################
## make a list of all files

file-list : FORCE
ifndef MADE_FROM_ABOVE
	@$(RM) -f FileList
endif
ifdef FILES
	@for i in $(FILES) ; \
	do  \
	   echo $(DIRNAME)/$$i ; \
	done >>$(TOP)/FileList 
endif
ifdef ALL_DIRS
	@for i in $(ALL_DIRS) ; \
	do \
	   echo "file-list in $(DIRNAME)/$$i" ;\
	   $(MAKE) --no-print-directory -C $$i MADE_FROM_ABOVE=1 NO_DEPEND=1 file-list ; \
	done
endif

FileList: file-list

###########################################################################
## list things which doc++ should process

doc++files : FORCE
	@echo "doc++files in $(DIRNAME)"
ifdef H
	@for i in $(H) ; \
	do  \
	   echo $(DIRNAME)/$$i ; \
	done >>$(TOP)/Doc++Files
endif
ifdef DOCXX_FILES
	@for i in $(DOCXX_FILES) ; \
	do  \
	   echo $(DIRNAME)/$$i ; \
	done >>$(TOP)/Doc++Files
endif
ifdef ALL_DIRS
	@for i in $(ALL_DIRS) ; \
	do \
	   $(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $$i doc++files ; \
	done
endif

###########################################################################
## Instalation rules

install_all: $(INSTALL) nothing
ifdef ALL_DIRS
	@ for i in $(ALL_DIRS) ; \
	do \
	   $(MAKE) --no-print-directory -C $$i install_all ; \
	done
endif

###########################################################################
## Target for directories which should never be entered on a normal build.

not_a_build_dir: FORCE
	@echo "+--------------------------------------------------"
	@echo "| This is not a build directory
	@echo "| plain make should never be run here
	@echo "+--------------------------------------------------"
	@exit 1

###########################################################################
## make info

info: 
ifeq ($(TOP),.)
	@$(RM) -f $(TOP)/config/modincludes* $(TOP)/config/system.mak
	@$(MAKE) -C $(TOP) --no-print-directory real-info .config_error
else
	@$(MAKE) -C $(TOP) --no-print-directory info
endif

