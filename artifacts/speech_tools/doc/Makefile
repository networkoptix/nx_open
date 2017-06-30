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
##                     Author :  Paul Taylor and Alan W Black            ##
##                     Date   :  July 1997                               ##
##-----------------------------------------------------------------------##
##                                                                       ##
##                    Documentation directory                            ##
##                                                                       ##
##  After Sergio's updates, make doc runs again.  You will need to have  ##
##  doxygen, xsltproc and graphviz installed to run this                 ##
##                                                                       ##
##=======================================================================##
TOP=..
DIRNAME=doc
ALL_DIRS = man images


#EXAMPLE_TO_DOCUMENT=matrix ling track sigpr deq error handle hash kvl \
#	list matrix named_enum pathname string token xml

IMAGES = cstr.gif edcrest.gif est.jpg

DOXYGEN_DOCUMENTATION = bibliography.bib estbaseclass.dox estexec.md \
        estgram.md estintro.md estlicence.md estling.md estmanuals.md \
        estserver.md estsigpr.md estspeechclass.md esttilt.md \
        estutil.md estwagon.md estxml.md index.md

FILES = DoxyFile $(DOXYGEN_DOCUMENTATION) $(IMAGES) Makefile

LOCAL_CLEAN = doxygen

-include $(TOP)/config/common_make_rules

doc:
	(cd $(TOP)/doc/man; make all_dox)
	doxygen $(TOP)/doc/DoxyFile


