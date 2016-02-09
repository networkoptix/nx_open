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
 ## --------------------------------------------------------------------  ##
 ## Rules for installing a project.                                       ##
 ##                                                                       ##
 ###########################################################################



PROJECT_HOME_PATH := $(shell mkdir -p $($(PROJECT_PREFIX)_HOME); cd $($(PROJECT_PREFIX)_HOME); pwd)

PROJECT_TOP_PATH := $(shell (cd $(TOP); pwd))

S = _static
ifdef SHARED
ifneq ($(SHARED),0)
	S = _shared 
endif
endif

ifeq ($(PROJECT_HOME_PATH), $(PROJECT_TOP_PATH))
    TO_INSTALL=make_bin
else

    TO_INSTALL= \
	make_installed_exec$(S) \
	make_installed_bin$(S) \
	make_installed_lib$(S)

    INSTALLED_LIB=$(shell (cd $($(PROJECT_PREFIX)_HOME); pwd))/lib
    INSTALLED_PRIVATE_LIB=$(INSTALLED_LIB)/$(shell echo $(PROJECT_PREFIX)| tr A-Z a-z)
    INSTALLED_BIN=$(shell (cd $($(PROJECT_PREFIX)_HOME); pwd))/bin
endif

install: $(TO_INSTALL)

make_bin: 
	@echo Making in bin
	@$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $(TOP)/bin

make_installed_exec_static:
	@: nothing

make_installed_exec_shared:
ifdef PROJECT_MAIN_DIR
	mkdir -p $(INSTALLED_PRIVATE_LIB)
	$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $(PROJECT_MAIN_DIR) BIN=$(INSTALLED_PRIVATE_LIB) BIN_TOP=$(TOP) .copy_main
else
	@:
endif


make_installed_bin_static:
	@mkdir -p $(INSTALLED_BIN)
	@$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $(PROJECT_SCRIPTS_DIR) BIN=$(INSTALLED_BIN) LIBDIR=$(INSTALLED_LIB) MAIN=$(INSTALLED_PRIVATE_LIB) .process_scripts_real
	@$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $(PROJECT_MAIN_DIR) BIN=$(INSTALLED_BIN) BIN_TOP=$(TOP) .copy_main


make_installed_bin_shared:
	@mkdir -p $(INSTALLED_BIN)
	@$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $(PROJECT_SCRIPTS_DIR) BIN=$(INSTALLED_BIN) LIBDIR=$(INSTALLED_LIB) MAIN=$(INSTALLED_PRIVATE_LIB) .process_scripts_real
ifdef PROJECT_MAIN_DIR
	@mkdir -p $(INSTALLED_BIN)
	@$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $(PROJECT_MAIN_DIR) BIN=$(INSTALLED_BIN) LIBDIR=$(INSTALLED_LIB) MAIN=$(INSTALLED_PRIVATE_LIB) .link_main$(S)
else
	@:
endif

make_installed_lib_static:
	@: nothing

make_installed_lib_shared:
	@mkdir -p $(INSTALLED_LIB)
	@$(MAKE) MADE_FROM_ABOVE=1 --no-print-directory -C $(PROJECT_LIB_DIR) LIB=$(INSTALLED_LIB) .copy_libs

