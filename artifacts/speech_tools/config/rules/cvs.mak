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

ifdef S
    SUBDIRARG=-R
else
    SUBDIRARG=-l
endif

ifndef CVS
    CVS=cvs
endif

# make sure we have the latest of everything

update:
	@ echo "Updating in directory $(DIRNAME) ..."
	$(CVS) update -d

# check in writable files

checkin_add: 
	@if [ -d CVS ] ; then \
	    echo "Registering new files in $(DIRNAME) ..." ;\
	    for f in $(FILES) ; do \
		if fgrep -q "/$$f/" CVS/Entries ;\
			then \
			: ;\
		else \
			echo Adding "$$f" ;\
			$(CVS) add "$$f" ;\
		fi ;\
	    done ;\
	    echo "Registering new directories in $(DIRNAME) ..." ;\
	fi	    
ifdef ALL_DIRS
	@if [ -d CVS ] ; then \
	    for d in $(ALL_DIRS) ; do \
		if egrep -q "^D/$$d/" CVS/Entries ;\
			then \
			: ;\
		else \
			echo Adding "$$d" ;\
			$(CVS) add "$$d" ;\
		fi ;\
	    done ;\
	fi
	@for i in $(ALL_DIRS) ; \
		do \
		$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $$i checkin_add ; \
	done
endif

checkin: checkin_add commit
ifeq ($(TOP),.)
	@echo Make sure file watching is turned on everywhere
	@$(CVS) watch on .
endif
	@:

commit:
	@ echo "Committing in directory $(DIRNAME) ..."
	@$(CVS) commit 

# Look for locked files

locked: 
	@ echo "Looking for files you are editing ..."
	@$(CVS) editors $(SUBDIRARG)  | egrep $${USER-$$LOGNAME} || true

editors:
	@ echo "Looking for files anyone are editing ..."
	@$(CVS) editors $(SUBDIRARG)

# look for locked by anyone

all_locked: editors
	@:


