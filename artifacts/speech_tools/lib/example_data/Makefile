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
 ##                                                                       ##
 ##                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             ##
 ## --------------------------------------------------------------------  ##
 ## This directory contains data used in tests and anything else which    ##
 ## might be useful to users trying things out.                           ##
 ##                                                                       ##
 ###########################################################################

TOP=../..
DIRNAME=lib/example_data

VITFILES = vit.B.ngram vit.observes vit.vocab
CH_WAVE_FILES = ch_wave.wav key.lab
CH_TRACK_FILES = ch_track.htk
LPC_FILES = kdt_001.wav kdt_001.pm
WAGON_FILES = wagon.data wagon.desc wagon.tree
TEXT_FILES = alice options.file
MAP_FILES = colours.map colours_translation.map channel_names.map
XML_FILES = eg.xml eg.dtd
UTT_FILES = ked_wr1_012.utt
TILT_FILES = kdt_001.il kdt_001.tilt
TESTFILES = $(VITFILES) $(CH_WAVE_FILES) $(CH_TRACK_FILES) $(TEXT_FILES) \
	$(MAP_FILES) $(LPC_FILES) $(WAGON_FILES) $(XML_FILES) $(UTT_FILES) \
	$(TILT_FILES)

FILES = Makefile $(TESTFILES)

include $(TOP)/config/common_make_rules
