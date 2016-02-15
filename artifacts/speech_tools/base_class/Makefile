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
#                     Makefile for base classes                           #
#                                                                         #
#=========================================================================#
TOP=..
DIRNAME=base_class

LIB_BUILD_DIRS = string inst_tmpl
BUILD_DIRS = $(LIB_BUILD_DIRS)

ALL_DIRS = $(BUILD_DIRS)

LOCAL_DEFAULT_LIBRARY = estbase

H = EST_matrix_support.h rateconv.h EST_get_function_template.h

T = EST_TList.cc EST_TSortable.cc EST_TKVL.cc \
	EST_TMatrix.cc EST_TSimpleMatrix.cc \
	EST_TVector.cc EST_TSimpleVector.cc \
	EST_THash.cc EST_Tvectlist.cc \
	EST_TTimeIndex.cc EST_TDeque.cc \
        EST_TBuffer.cc EST_TNamedEnum.cc

TSRCS =  EST_Featured.cc EST_Features.cc

CPPSRCS = EST_UList.cc EST_Option.cc EST_StringTrie.cc EST_Token.cc \
	vec_mat_aux.cc EST_Pathname_unix.cc THash_aux.cc \
	EST_FMatrix.cc EST_Complex.cc EST_Val.cc \
	EST_matrix_support.cc rateconv.cc \
        EST_IMatrix.cc EST_SMatrix.cc \
        EST_DMatrix.cc vec_mat_aux_d.cc EST_FeatureData.cc\
        EST_slist_aux.cc EST_svec_aux.cc EST_ilist_aux.cc \
	EST_features_aux.cc EST_features_io.cc vec_mat_aux_i.cc \
	$(TSRCS)

SRCS = $(CPPSRCS)
OBJS = $(CPPSRCS:.cc=.o)
FILES = $(SRCS) EST_Pathname_win32.cc $(T) $(H)\
	Makefile

ALL = .buildlib $(BUILD_DIRS)

include $(TOP)/config/common_make_rules

# Always compile with -O3 as its an important function
rateconv.o:
	$(CXX_COMMAND) -O3 rateconv.cc -o rateconv.o
