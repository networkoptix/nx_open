 ###########################################################################
 ##                                                                       ##
 ##                Centre for Speech Technology Research                  ##
 ##                     University of Edinburgh, UK                       ##
 ##                         Copyright (c) 1996-2003                       ##
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
 ##                       Author: Korin Richmond                          ##
 ##                         Date: 15 Apr 2003                             ##
 ## --------------------------------------------------------------------  ##
 ##                                                                       ##
 ## Make definitions for the module which builds bindings for high level  ##
 ## script languages, such as Python, for selected EST classes and          ##
 ## functionality.  This makes use of SWIG                                ##
 ##                                                                       ##
 ###########################################################################


INCLUDE_WRAPPERS = 1

MOD_DESC_WRAPPERS = Script language bindings for EST libraries 

# not sure what magic it would take to put the contents of the
# following ifeq block into $(TOP)/wrappers/Makefile, but in some
# ways it would be nicer...
ifeq ($(DIRNAME),wrappers)
    ifndef CONFIG_SWIG_COMPILER
     .config_error:: FORCE
	    @echo "+--------------------------------------------------"
	    @echo "| Please specify path to swig binary in config/config"
	    @echo "+--------------------------------------------------"
	    @exit 1
    endif

    ifeq ($(CONFIG_SWIG_COMPILER),none)
    .config_error:: FORCE
	    @echo "+--------------------------------------------------"
	    @echo "|  Please specify path to swig binary in config/config"
	    @echo "+--------------------------------------------------"
	    @exit 1
    endif

    ifneq ($(findstring PYTHON,$(CONFIG_WRAPPER_LANGUAGES)),)
        BUILD_DIRS+=python
    endif

endif

ifeq ($(DIRNAME),.)
    EXTRA_BUILD_DIRS := $(EXTRA_BUILD_DIRS) wrappers
endif



