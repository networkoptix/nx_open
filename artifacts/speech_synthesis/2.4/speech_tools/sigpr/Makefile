###########################################################################
##                                                                       ##
##                Centre for Speech Technology Research                  ##
##                     University of Edinburgh, UK                       ##
##                   Copyright (c) 1994,1995,1996                        ##
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
#                      Signal processing Makefile                         #
#                                                                         #
#=========================================================================#
TOP=..
DIRNAME=sigpr

LIB_BUILD_DIRS = pda 
BUILD_DIRS = $(LIB_BUILD_DIRS)
ALL_DIRS = $(BUILD_DIRS) # sigpr_development

LOCAL_DEFAULT_LIBRARY = estbase
H = 

TSRCS = EST_Window.cc
# Some versions of gcc can't optimize this file (most can)
NOOPTSRCS = fft.cc 
CPPSRCS = delta.cc filter.cc \
	sigpr_frame.cc sigpr_utt.cc \
	pitchmark.cc spectrogram.cc misc.cc $(NOOPTSRCS)

SRCS = $(TSRCS) $(CPPSRCS)
OBJS = $(SRCS:.cc=.o)
DOCXX = lpc.cc
FILES = $(SRCS) $(H) Makefile

ALL = .buildlib $(BUILD_DIRS)

include $(TOP)/config/common_make_rules



