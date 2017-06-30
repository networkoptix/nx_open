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
##             Linguistic Class Makefile                                 ##
##=======================================================================##

TOP=..
DIRNAME=ling_class

LOCAL_DEFAULT_LIBRARY = estbase

H = EST_UtteranceFile.h genxml.h apml.h solexml.h relation_io.h ling_class_init.h \
	  EST_FeatureFunctionContext.h

TSRCS =   ling_t.cc EST_Utterance.cc \
          item_feats.cc apml.cc solexml.cc  EST_FeatureFunctionPackage.cc \
	  EST_FeatureFunctionContext.cc

LOCAL_TSRCS = EST_UtteranceFile.cc genxml.cc EST_utterance_aux.cc ling_class_init.cc 
CPPSRCS = EST_Item.cc EST_Item_Content.cc item_aux.cc \
          EST_relation_aux.cc EST_relation_track.cc \
          relation_io.cc EST_relation_compare.cc \
	  standard_feature_functions.cc EST_Relation_mls.cc \
	  EST_item_aux.cc EST_item_content_aux.cc EST_Relation.cc \
          $(LOCAL_TSRCS) $(TSRCS)

SRCS = $(CPPSRCS)
OBJS = $(CPPSRCS:.cc=.o)
FILES = $(SRCS) $(H) Makefile

VC_LOCAL_DEFINES = /DESTLIBDIRC=$(EST_HOME)/lib /DINCLUDE_XML_FORMATS=1

ALL = .buildlib

include $(TOP)/config/common_make_rules

ifdef INCLUDE_RXP
    UTTERANCEFILEDEFS += -DINCLUDE_XML_FORMATS
endif

ling_class_init.o EST_utterance_aux.o EST_UtteranceFile.o: %.o : %.cc
	$(strip $(CXX_COMMAND_TEMPLATES) $(UTTERANCEFILEDEFS) $< )

genxml.o: genxml.cc
	$(strip $(CXX_COMMAND_TEMPLATES) -DESTLIBDIRC='$(EST_HOME)/lib' $< )
