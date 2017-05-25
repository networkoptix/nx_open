###########################################################################
##                                                                       ##
##                Centre for Speech Technology Research                  ##
##                     University of Edinburgh, UK                       ##
##                    Copyright (c) 1994,1995,1996                       ##
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
#                       Author :  Paul Taylor                             #
#                       Date   :  June 1994                               #
#-------------------------------------------------------------------------#
#           Makefile for directory for stand alone programs               #
#                                                                         #
#=========================================================================#
TOP=..
DIRNAME=main
ALL_DIRS = # main_development
BUILD_DIRS = 
H = 
TSRCS =	 
OLD_BASESRCS = fringe_client_main.cc 
BASESRCS = ch_lab_main.cc ch_track_main.cc ch_wave_main.cc \
           na_play_main.cc na_record_main.cc \
           sigfilter_main.cc sig2fv_main.cc spectgen_main.cc \
           pda_main.cc pitchmark_main.cc \
           ngram_test_main.cc ngram_build_main.cc \
	   tilt_synthesis_main.cc tilt_analysis_main.cc \
	   viterbi_main.cc dp_main.cc bcat_main.cc ch_utt_main.cc \
	   design_filter_main.cc align_main.cc


MAINSRCS = $(BASESRCS)
MODMAINSRCS =

# Part of the distribution but only optionally compiled
EXTRASRCS = wagon_main.cc wagon_test_main.cc \
            ols_main.cc ols_test_main.cc \
            scfg_make_main.cc scfg_train_main.cc scfg_test_main.cc \
            scfg_parse_main.cc \
            wfst_build_main.cc wfst_run_main.cc wfst_train_main.cc \
            siod_main.cc \
            xml_parser_main.cc


CPPSRCS = $(MAINSRCS) $(TSRCS) $(EXTRASRCS)

OBJS = $(CPPSRCS:.cc=.o)

# Specific extra LIBS for specific programs
na_play_LIBS = $(AUDIO_LIBS) $(AUDIO_EXTRA_LIBS)
ch_lab_LIBS = $(FILEFORMATLIBS)
ch_track_LIBS = $(FILEFORMATLIBS)

MAIN_EXECS = $(MAINSRCS:_main.cc=)
MOD_EXECS = $(notdir $(MODMAINSRCS:_main.cc=))
ALL_EXECS = $(MAIN_EXECS) $(MOD_EXECS)
ALL = .make_execs .sub_directories

SRCS = $(CPPSRCS)
FILES= $(BASESRCS) $(EXTRASRCS) $(TSRCS) Makefile

include $(TOP)/config/common_make_rules
include $(TOP)/config/rules/bin_process.mak

ifeq (SIOD_PYTHON, $(findstring SIOD_PYTHON, $(INCLUDE_MODULES)))
	MODULE_LIBS +=$(shell python3-config --ldflags) -ldl -lutil -lm
endif

MAIN_O = $(MAIN_EXECS:%=%_main.o)
$(MAIN_O) : %_main.o : %_main.cc
	$(CXX_COMMAND) -o $@ $<

MOD_O = $(MOD_EXECS:=_main.o)
$(MOD_O) : %_main.o : $(SRC_FOR_%)
	$(CXX_COMMAND) -o $@ -DESTLIBDIR=\"$(EST_HOME)/lib\" $($*_SRC)

$(ALL_EXECS) : % : $(LIBDEPS) %_main.o 
	$(strip $(LINK_COMMAND) -o $@ $@_main.o $(LIBS) $($(@:=_LIBS)))

.make_execs : $(ALL_EXECS) 


