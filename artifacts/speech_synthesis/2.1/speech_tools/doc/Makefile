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
##=======================================================================##
TOP=..
DIRNAME=doc
ALL_DIRS = man arch_doc

DOCNAME=speechtools

DSSL_SANE_DB=sane_to_docbook.dsl
DSSSL_HTML=cstr.dssl
DSSSL=$(DSSSL_HTML)
TEX_SCALE=1.5

# Temporarilly we explicitly list the programs which have been documented

MAIN_TO_DOCUMENT=ch_wave ch_track ch_lab tilt_analysis tilt_synthesis sig2fv \
	sigfilter pda pitchmark spectgen design_filter dp ngram_build \
	na_play ngram_test viterbi wagon xml_parser wfst_build wfst_run \
	scfg_make scfg_train scfg_parse scfg_test siod ols ols_test \
	fringe_client bcat na_record

EXAMPLE_TO_DOCUMENT=matrix ling track sigpr deq error handle hash kvl \
	list matrix named_enum pathname string token xml

SGMLFILES = cstr_defs.dtd speechtools.sgml estbaseclass.sgml estexec.sgml \
	estgram.sgml esttilt.sgml estxml.sgml estserver.sgml estjavaapp.sgml \
	estspeechclass.sgml estsigpr.sgml estintro.sgml estint.sgml \
        estwagon.sgml estutil.sgml

DSSSLFILES = $(DSSSL_HTML) $(DSSL_SANE_DB)

DOCXXIMAGES = edcrest.gif cstr.gif est.jpg est_small.jpg
DOCXXFILES=  classHeader.inc hierHeader.inc indexHeader.inc \
	     banner.inc $(DOCXXIMAGES)

HTML_REQUIREMENTS = $(DOCXXIMAGES) cstrdoc.css arch_doc

FILES = $(DOCXXFILES) cstrdoc.css sane.dtd \
	$(SGMLFILES) Makefile $(DSSSLFILES)
LOCAL_CLEAN = *.aux *.cp *.fn *.ky *.log *.pg *.toc *.tp *.vr

ALL=new_doc_help

include $(TOP)/config/common_make_rules
include $(TOP)/config/rules/doc.mak











