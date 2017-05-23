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
 ## Make rules dealing with RCS.                                          ##
 ##                                                                       ##
 ###########################################################################

# Pass down an argument which says who we are

LOCKED_FILTER=-l$(USER),$(LOGNAME)

ifndef CI_ARGS
	CI_ARGS=-u
endif

ifdef LOCAL_REPOSITORY
    MAKE_RCSED_DIR = ( cd XXX ; ln -s  $(CENTRAL_DIR)/$(DIRNAME)/XXX/RCS ; co Makefile ) 
else
    MAKE_RCSED_DIR = mkdir XXX/RCS
endif


# make sure we have the latest of everything

update:
	@ echo "Updating in directory $(DIRNAME) ..."
	@if $(GNUTEST) RCS/Makefile,v -nt Makefile ; then \
		co Makefile ;\
		$(MAKE) update ;\
	else \
		for i in $(FILES) ; \
		   do \
		   $(GNUTEST) -f "$$i"  -a  "$$i" -nt "RCS/$$i,v"  || co "$$i" ;\
		done ;\
	fi
ifdef ALL_DIRS
	@for i in $(ALL_DIRS) ; \
		do \
		if [ ! -f $$i/Makefile ] ;\
			then \
			echo "Makeing new directory $(DIRNAME)/$$i" ;\
			mkdir $$i ;\
			$(subst XXX,$$i,$(MAKE_RCSED_DIR)) ;\
		fi ;\
		$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $$i update ; \
	done
endif

new_update: 
	@$(MAKE) --no-print-directory  INCLUDE_RCS_DEPEND=1 MADE_FROM_ABOVE=1 -k do_new_update

ifdef INCLUDE_RCS_DEPEND

do_new_update: do_new_update_message $(FILES) FORCE
	@$(DO_NOTHING)
ifdef ALL_DIRS
	@ for i in $(ALL_DIRS) ; \
		do \
		$(MAKE) --no-print-directory INCLUDE_RCS_DEPEND=1 -C $$i do_new_update ; \
	done
endif

do_new_update_message: FORCE
	@ echo "Updating in directory $(DIRNAME) ..."

$(FILES) : % : RCS/%,v
	co $<
endif

# check in writable files

checkin:
	@ echo "Checking in directory $(DIRNAME) ..."
	@for i in $(FILES) ; \
	do \
	   if $(TEST) -w $$i ; then \
	      ci $(CI_ARGS) $$i; \
	   fi; \
	done
ifdef ALL_DIRS
	@ for i in $(ALL_DIRS) ; \
	do \
	   $(MAKE) --no-print-directory -C $$i checkin ; \
	done
endif

# Look for locked files

locked:
	@locked=`rlog -L -R $(LOCKED_FILTER) $(FILES)` ;\
	if $(TEST) -n "$$locked" ; then \
		 echo "Locked in directory $(DIRNAME) ..."; \
		for i in $$locked ; \
		do \
			rlog -h $$i | awk '$$1 == "Working" {f=$$3} $$1=="locks:" {n=1;next} n==1 {w=$$1;n=0} END {print w "\t" f}' - ;\
		done ;\
	else \
		echo "Nothing in $(DIRNAME) ..." ;\
	fi
ifdef ALL_DIRS
	@ for i in $(ALL_DIRS) ; \
	do \
	   $(MAKE) --no-print-directory -C $$i locked ; \
	done
endif

# look for locked by anyone

all_locked:
	@$(MAKE) --no-print-directory LOCKED_FILTER= locked


