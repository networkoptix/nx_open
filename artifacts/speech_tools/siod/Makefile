###########################################################################
##                                                                       ##
##                Centre for Speech Technology Research                  ##
##                     University of Edinburgh, UK                       ##
##                       Copyright (c) 1996,1997                         ##
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
#                                                                         #
# Makefile for siod directory                                             #
#                                                                         #
# Note the siod code in this directory was mostly developed by            #
# PARADIGM ASSOCIATES INCORPORATED, CAMBRIDGE, MASSACHUSETTS.             #
# and hence falls under their copyright                                   #
#                                                                         #
# The editline library comes from Simmule Turner and Rich Salz copyright  #
# 1992 all rights reserved, it was inserted in the standard speech tools  #
# distribution in Dec 1998                                                #
#                                                                         #
# Note that almost all of the files in this directory are free with no    #
# commercial restriction as the as so heavily based on existing free      #
# software                                                                #
#                                                                         #
###########################################################################
TOP=..
DIRNAME=siod
EDITLINEH = siodeditline.h editline.h el_unix.h
OLD_H = EST_SiodServer.h 
H = siodp.h io.h slib_python.h $(EDITLINEH)
TSRCS = siod.cc siod_est.cc
ELSRCS = siodeditline.c el_complete.c editline.c el_sys_unix.c
CSRCS =  siodeditline.c 
OLD_CXXSRCS =  siod_server.cc EST_SiodServer.cc siod_fringe.cc
CXXSRCS = slib.cc \
          slib_core.cc slib_doc.cc slib_file.cc slib_format.cc \
          slib_list.cc slib_math.cc slib_sys.cc slib_server.cc \
          slib_str.cc slib_xtr.cc slib_repl.cc \
          slib_python.cc \
          io.cc trace.cc $(TSRCS)
SRCS = $(CSRCS) $(CXXSRCS)

OBJS = $(CXXSRCS:.cc=.o) $(CSRCS:.c=.o) 

FILES=Makefile siod.mak el_sys_win32.c $(CXXSRCS) $(ELSRCS) $(H)

ALL = .buildlib

include $(TOP)/config/common_make_rules

DEFINES  += $(IO_DEFINES)
INCLUDES += $(IO_INCLUDES)

ifeq (SIOD_PYTHON, $(findstring SIOD_PYTHON, $(INCLUDE_MODULES)))
	MODULE_INCLUDES += $(shell python3-config --includes)
	MODULE_DEFINES += -DEST_SIOD_ENABLE_PYTHON
endif

