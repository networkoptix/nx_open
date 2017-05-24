 ###########################################################################
 ##                                                                       ##
 ##                Centre for Speech Technology Research                  ##
 ##                     University of Edinburgh, UK                       ##
 ##                         Copyright (c) 1997                            ##
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
 ##                   Testsuite Directory Makefile                        ##
 ###########################################################################

 # To quickly rebuld the tests:
 #	make
 # To run the tests
 #	make test
 # To run a specific test
 #	make hash_module_build_and_test
 # or just
 #	make hash_module_rebuild
 #	make hash_module_test


TOP=..
DIRNAME=testsuite
ALL_DIRS = correct

# Place to find test data

DATA=$(EST_HOME)/lib/example_data

VC_LOCAL_DEFINES = /DDATAC=$(EST_HOME)/lib

# tests performed by shell scripts
TEST_SCRIPTS = ch_wave ch_track viterbi wagon tilt lpc

# tests performed by C++ programs
#TEST_MODULES = string list hash pathname named_enum track_map token
TEST_MODULES = string hash pathname \
	track token complex list kvl \
	handle named_enum deq ling feature
TMP_OMITTED = track_map error

# These don't compile on some systems (due to compiler problems)
OTHERS=sigpr_example.cc sigpr_regression.cc

# define any arguments for the scripts or programs here (for instance
# if they need to be passed the name of a data file).

hash_regression_args = $(DATA)/alice

TEST_OPTIMISE=3

# by default we just rebuild the C++ tests to catch typos etc.
ALL = build_test_programs
ALL_EXECS = $(TEST_MODULES:%=%_example) $(TEST_MODULES:%=%_regression)
LOCAL_CLEAN = tmp

include $(TOP)/config/test_make_rules

ifdef INCLUDE_RXP
	ALL = foobar $(shell echo help>/dev/tty)
endif

MODULES = $(TEST_MODULES:%=%_example)

foo:
	: $(ALL_EXECS)

OPTIMISE=$(TEST_OPTIMISE)

LOCAL_DEFINES +=-DTVECTOR_BOUNDS_CHECKING

override WARN=1

just_test test: 
	$(MAKE) --no-print-directory $@s 2>&1 | tee .test_out
	@if egrep 'FAILED|INCORRECT|Warning' .test_out >/dev/null;\
		then \
		echo "-------------" ;\
		echo "Tests failed:" ;\
		egrep 'FAILED|INCORRECT|Warning' .test_out ;\
	else \
		echo "-------------" ;\
		echo "Test OK" ;\
	fi
tmpdir:
	rm -rf tmp
	mkdir tmp

tests: tmpdir test_modules test_scripts

just_tests: tmpdir just_test_modules just_test_scripts


build_test_programs: $(TEST_PROGRAMS)
	@:
