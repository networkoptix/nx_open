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
 ##                   Date: Wed Mar  4 1998                               ##
 ## --------------------------------------------------------------------  ##
 ## Rules for creating the bin directory from scripts and main.           ##
 ##                                                                       ##
 ###########################################################################

S=_static

ifdef SHARED
ifneq ($(SHARED),0)
	S = _shared 
endif
endif

ifndef LD_LIBRARY_PATH_VARIABLE
    LD_LIBRARY_PATH_VARIABLE = LD_LIBRARY_PATH
endif

.remove_links: FORCE
	@echo
	@$(ECHO_N) "Remove Links:"
	@for i in * ;\
	    do \
	    case "$$i" in \
		Makefile ) : ;; \
		VCLocalRules ) : ;; \
		RCS|CVS ) : ;; \
		* ) $(ECHO_N) " $$i"; $(RM) -f "$$i";; \
	    esac \
	done
	@echo


.copy_main: FORCE
	@echo
	@$(ECHO_N) "Install Executables:"
	@main=`pwd` ;\
	for i in $(ALL_EXECS) ;\
	    do \
	    b=`basename "$$i"`;\
	    $(ECHO_N) " $$b";\
	    $(INSTALL_PROG) -s "$$main/$$i" "$(subst TOP,$(TOP),$(BIN))/$$b";\
	done
	@echo

.link_main: 
	@$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C '$(PROJECT_MAIN_DIR)' BIN=TOP/$(DIRNAME) LIBDIR=MAIN/TOP/$(PROJECT_LIBRARY_DIR) MAIN='$($(PROJECT_PREFIX)_HOME)' .link_main$(S)

.link_main_static: FORCE
	@echo
	@$(ECHO_N) "Main Links:"
	@for i in $(ALL_EXECS) ;\
	    do \
	    b=`basename "$$i"`;\
	    $(ECHO_N) " $$b";\
	    ln -s "$(MAIN)/$(DIRNAME)/$$i" "$(subst TOP,$(TOP),$(BIN))/$$b";\
	done
	@echo

.link_main_shared: FORCE
	@echo
	@$(ECHO_N) "Main Scripts:"
	@for i in $(ALL_EXECS) ;\
	    do \
	    b=`basename "$$i"`;\
	    $(ECHO_N) " $$b"; \
	    $(NAWK) -f "$(EST)/config/rules/script_process.awk" \
				scriptname="$$b" \
				project="$(PROJECT_NAME)" \
				version="$(PROJECT_VERSION)" \
				systemtype="$(MACHINETYPE)_$(OSTYPE)$(OSREV)" \
				topdir="$($(PROJECT_PREFIX)_HOME)" \
				main="$(MAIN)/$(DIRNAME)" \
				lib="$($(PROJECT_PREFIX)_HOME)/$(PROJECT_LIBRARY_DIR)" \
				est="'$(EST_HOME)'" \
				classpath="$(SYSTEM_JAVA_CLASSPATH)" \
				perl="$(PERL)" \
				javahome="$(JAVA_HOME)" \
				java="$(JAVA)" \
				javac="$(JAVAC)" \
				java_version="$(EST_JAVA_VERSION)" \
				ldpath="$(SYSTEM_LD_LIBRARY_PATH)" \
				ldvar="$(LD_LIBRARY_PATH_VARIABLE)" \
				$(PROJECT_SCRIPTS_DIR)/shared_script  > "$(subst TOP,$(TOP),$(BIN))/$$b" ;\
	    chmod +x "$(subst TOP,$(TOP),$(BIN))/$$b" ;\
	done
	@echo


.process_scripts: 
	@$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C '$(PROJECT_SCRIPTS_DIR)' BIN=TOP/$(DIRNAME) BIN_TOP='$(TOP)' .process_scripts_real

.process_scripts_real: FORCE	
	@echo
	@$(ECHO_N) "Scripts:"
	@for ex in sh prl ;\
		do \
		$(ECHO_N) " ($$ex)" ;\
		$(NAWK) -f "$(EST)/config/rules/script_process.awk" \
			topdir="$($(PROJECT_PREFIX)_HOME)" \
			est="$(EST_HOME)" \
			ldpath="$(SYSTEM_LD_LIBRARY_PATH)" \
			"$(PROJECT_SCRIPTS_DIR)"/shared_setup_$$ex > /tmp/$$$$.$$ex ;\
	done ;\
	for script in alwaysone $(SCRIPTS) ;\
		do \
		if [ $$script != alwaysone ] ;\
		then \
		ex=`expr "$$script" : '.*\.\([^.]*\)$$'` ;\
		b=`basename "$$script" .$$ex`;\
		$(ECHO_N) " $$b"; \
		$(NAWK) -f "$(EST)/config/rules/script_process.awk" \
			project="$(PROJECT_NAME)" \
			version="$(PROJECT_VERSION)" \
			systemtype="$(MACHINETYPE)_$(OSTYPE)$(OSREV)" \
			shared="$(S)" \
			sharedsetup="/tmp/$$$$.$$ex" \
			ext="$$ex" \
			scriptname="$$b" \
			topdir="$($(PROJECT_PREFIX)_HOME)" \
			est="'$(EST_HOME)'" \
			perl="$(PERL)" \
			javahome=$(JAVA_HOME) \
			java_version="$(EST_JAVA_VERSION)" \
			java=$(JAVA) \
			javac=$(JAVAC) \
			classpath=$(SYSTEM_JAVA_CLASSPATH) \
			ldpath="$(SYSTEM_LD_LIBRARY_PATH)" \
			ldvar="$(LD_LIBRARY_PATH_VARIABLE)" \
			"$$script"  > "$(subst TOP,$(TOP),$(BIN))/$$b" ;\
		chmod +x "$(subst TOP,$(TOP),$(BIN))/$$b";\
		fi ;\
	done ;\
	$(RM) -f /tmp/$$$$.*
	@echo


