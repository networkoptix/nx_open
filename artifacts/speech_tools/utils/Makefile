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
#                     Makefile for utility classes                        #
#                                                                         #
#=========================================================================#
TOP=..
DIRNAME=utils

LOCAL_DEFAULT_LIBRARY = estbase

H = 
OLD_TSRCS =  EST_ServiceTable.cc
TSRCS = est_file.cc 
CSRCS = EST_cutils.c EST_error.c walloc.c system_specific_unix.c 
OLD_CPPSRCS = EST_Server.cc EST_FringeServer.cc 
CPPSRCS = cmd_line.cc util_io.cc filetrans.cc cmd_line_aux.cc  \
          EST_swapping.cc $(TSRCS)

SRCS = $(CPPSRCS) $(CSRCS)
OBJS = $(CPPSRCS:.cc=.o) $(CSRCS:.c=.o)
OBJS_eststring = walloc.o
FILES = $(SRCS) $(H) Makefile system_specific_win32.c

LOCAL_INCLUDES =

ALL = .buildlibs

include $(TOP)/config/common_make_rules

EST_cutils.o: EST_cutils.c
	$(CC_COMMAND) -DESTNAME='$(PROJECT_NAME)' -DESTDATE='$(PROJECT_DATE)' -DESTVERSION='$(PROJECT_VERSION)' -DESTLIBDIRC='$(EST_HOME)/lib' -DESTSTATE='$(PROJECT_STATE)' -DESTOSTYPE='$(SYSTEM_TYPE)' EST_cutils.c

