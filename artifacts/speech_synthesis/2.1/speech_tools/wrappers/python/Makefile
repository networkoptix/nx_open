###########################################################################
##                                                                       ##
##                Centre for Speech Technology Research                  ##
##                     University of Edinburgh, UK                       ##
##                         Copyright (c) 2003                            ##
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
#                       Author :  Korin Richmond                          #
#                       Date   :  April 2003                              #
#-------------------------------------------------------------------------#
#           Makefile for python bindings to speech tools                  #
#                                                                         #
#=========================================================================#

TOP=../..
DIRNAME=wrappers/python

WRAPPERS = EST_FVector.i \
	   EST_Wave.i EST_Track.i EST_SignalProc.i\
	   EST_pitchmark.i \
	   EST_Utterance.i EST_Relation.i EST_Item.i \
	   EST_Ngrammar.i #EST_Complex.i

WRAPPER_SRCS = $(WRAPPERS:.i=_wrap.cc)
WRAPPER_PY = $(WRAPPERS:.i=.py)
OBJS = $(WRAPPER_SRCS:.cc=.o)
ALL_WRAPPERS = $(WRAPPERS:%.i=_%.so)

FILES= Makefile $(WRAPPER_SRCS) $(WRAPPER_PY)
ALL = .make_wrappers
LOCAL_INCLUDES = $(CONFIG_PYTHON_INCLUDES)

# following flag switches off GCC's strict aliasing rules assumption
# when performing optimization (hence, that optimisiation is no
# longer available).  This is necessary because swig generated wrapper
# code contains lots of type punning in the course of doing its magic,
# which violates the strict aliasing and so can generate duff code 
CC_OTHER_FLAGS += -fno-strict-aliasing 

include $(TOP)/config/common_make_rules
include $(TOP)/config/rules/bin_process.mak

# if the user gives a path to swig in $EST/config/config file, then
# test *.i interface files, and use swig to rebuild wrappers, otherwise
# use the default pre-swigged versions (and not delete them on make clean!)
ifdef CONFIG_SWIG_COMPILER
  INTERFACE_DIRS = ../interface/ ../interface/python/ ../../include
  INTERFACE_DIRS_SWG = $(foreach d,$(INTERFACE_DIRS),-I$(d))
  vpath %.i $(INTERFACE_DIRS)
  SWIG_COMMAND = $(CONFIG_SWIG_COMPILER) -c++ -python -shadow -Wall

  $(WRAPPER_SRCS) : %_wrap.cc : %.i
	  $(SWIG_COMMAND) $(INTERFACE_DIRS_SWG) -o $@ $<

  LOCAL_CLEAN = $(WRAPPER_SRCS) $(WRAPPERS:.i=.py) $(WRAPPERS:.i=.pyc) $(ALL_WRAPPERS)
else
  LOCAL_CLEAN = $(WRAPPERS:.i=.pyc) $(ALL_WRAPPERS)
endif

$(ALL_WRAPPERS) : _%.so : %_wrap.o
	$(subst XXX,$@,$(MAKE_SHARED_LIB)) $(LIBS) $<

.make_wrappers : $(ALL_WRAPPERS) 


