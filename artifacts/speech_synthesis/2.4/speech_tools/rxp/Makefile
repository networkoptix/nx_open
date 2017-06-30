 ###########################################################################
 ##                                                                       ##
 ## Copyright (c) 1997-98 Richard Tobin, Language Technology Group, HCRC, ##
 ## University of Edinburgh.                                              ##
 ##                                                                       ##
 ## THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,     ##
 ## EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    ##
 ## MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT ##
 ## IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF EDINBURGH BE LIABLE ##
 ## FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF    ##
 ## CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION    ##
 ## WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       ##
 ##                                                                       ##
 ###########################################################################
 ##                                                                       ##
 ##  This directory contains an XML parser and trivial interface for its  ##
 ##  use in Speech Tools, Festival and Fringe.                            ##
 ##                                                                       ##
 ##                                                                       ##
 ##  This parser does not fall under Speech Tools licencing but is also   ##
 ##  free to be used for any purpose without fee.  Note that other        ##
 ##  versions of RXP (with differing functionality) are available from    ##
 ##  LTG, HCRC under different licences including the GPL.                ##
 ##                                                                       ##
 ###########################################################################

TOP=..
DIRNAME=rxp

H = 
SRCSC = charset.c dtd.c string16.c url.c \
        ctype16.c input.c stdio16.c system.c xmlparser.c

TSRCS = XML_Parser.cc

SRCS = $(SRCSC) $(TSRCS)

OBJS = $(SRCSC:.c=.o) $(TSRCS:.cc=.o) 

FILES = Makefile $(SRCS) $(H) rxp.c rxp.mak

LOCAL_INCLUDES = -I$(EST)/include/rxp

LOCAL_DEFINES =  -DCHAR_SIZE=8

LOCAL_DEFAULT_LIBRARY = estbase

VC_LOCAL_DEFINES= /DCHAR_SIZE=8

ALL = .buildlib

include $(TOP)/config/common_make_rules


