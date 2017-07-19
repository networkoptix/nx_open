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
##                       Speech Class Makefile                           ##
##                                                                       ##
##=======================================================================##
TOP=..
DIRNAME=speech_class

LOCAL_DEFAULT_LIBRARY = estbase

H = htk.h esps_utils.h waveP.h EST_wave_utils.h \
	EST_TrackFile.h EST_WaveFile.h ssff.h
TSRCS =   EST_TrackMap.cc EST_Track.cc \
          wave_t.cc track_t.cc EST_wave_aux.cc \
	  EST_TrackFile.cc EST_WaveFile.cc 
CPPSRCS = EST_Wave.cc EST_track_aux.cc \
          EST_wave_temp.cc EST_wave_cuts.cc ssff.cc \
	  esps_io.cc esps_utils.cc EST_wave_io.cc EST_wave_utils.cc \
          $(TSRCS)
#         EST_wave_stream.cc
SRCS = $(CPPSRCS) $(CSRCS)
OBJS = $(CPPSRCS:.cc=.o)

FILES = $(SRCS) $(H) Makefile

SHAREABLE_OBJECTS = $(OBJS)

ALL = .buildlib

include $(TOP)/config/common_make_rules



