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
#              Makefile for base class instantiations                     #
#                                                                         #
#   Keeping these as different .o files makes the binaries a lot smaller  #
#                                                                         #
#=========================================================================#
TOP=../..
DIRNAME=base_class/inst_tmpl

LOCAL_DEFAULT_LIBRARY = estbase

TSRCS = list_i_t.cc list_si_t.cc list_f_t.cc list_d_t.cc \
	list_c_t.cc list_s_t.cc \
        list_val_t.cc \
	list_li_t.cc list_vs_t.cc list_vi_t.cc \
	\
	matrix_i_t.cc matrix_si_t.cc matrix_f_t.cc matrix_d_t.cc \
	matrix_s_t.cc \
        matrix_val_t.cc \
	\
	vector_i_t.cc vector_si_t.cc vector_f_t.cc vector_d_t.cc \
	vector_c_t.cc vector_s_t.cc \
        vector_val_t.cc vector_ls_t.cc \
	vector_fvector_t.cc vector_fmatrix_t.cc \
	vector_dvector_t.cc vector_dmatrix_t.cc \
	\
	kvl_fi_t.cc kvl_ii_t.cc kvl_sd_t.cc kvl_sf_t.cc \
        kvl_ss_t.cc kvl_si_t.cc kvl_sv_t.cc kvl_rs_t.cc kvl_vpi_t.cc \
	\
	hash_fi_t.cc hash_ii_t.cc hash_sd_t.cc hash_sf_t.cc \
        hash_ss_t.cc hash_si_t.cc hash_sv_t.cc hash_iv_t.cc \
        hash_sfmp_t.cc hash_srp.cc \
	\
	tbuffer_t.cc \
	\
	deq_s_t.cc deq_i_t.cc \
	\

#	kvl_srcfmp_t.cc kvl_scsmm_t.cc matrix_c_t.cc 

CPPSRCS = $(TSRCS)

SRCS = $(CPPSRCS)
OBJS = $(CPPSRCS:.cc=.o)
FILES = $(SRCS) Makefile timeindex_tr.cc


ALL = .buildlib $(BUILD_DIRS)

include $(TOP)/config/common_make_rules

