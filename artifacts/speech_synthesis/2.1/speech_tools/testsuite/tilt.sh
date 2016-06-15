#!/bin/sh
###########################################################################
##                                                                       ##
##                Centre for Speech Technology Research                  ##
##                     University of Edinburgh, UK                       ##
##                         Copyright (c) 1999                            ##
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
PDA=$TOP/bin/pda
TILT_ANALYSIS=$TOP/bin/tilt_analysis
TILT_SYNTHESIS=$TOP/bin/tilt_synthesis
CH_TRACK=$TOP/bin/ch_track

DATA=$TOP/lib/example_data

tilt_test () {

	/bin/rm -f tmp/kdt_001.f0
	/bin/rm -f tmp/kdt_001.tilt
	/bin/rm -f tmp/kdt_001.tilt.f0

	echo "F0 extraction" >&2
	$PDA -shift 0.01 -o tmp/kdt_001.f0 -otype esps -fmax 180 -fmin 80 $DATA/kdt_001.wav
	$CH_TRACK -info tmp/kdt_001.f0
	echo

	echo "Tilt analysis" >&2
	$TILT_ANALYSIS -smooth -otype tilt -e $DATA/kdt_001.il tmp/kdt_001.f0 -w1 0.05 -w2 0.05 -o tmp/kdt_001.tilt -event_names "a afb"
	# Floats are different on different machines, do a hack to cover
        # Sun and Intel, may work for others too
        sed 's/0.0269821/0.026982/' tmp/kdt_001.tilt | diff - $DATA/kdt_001.tilt
	echo "Tilt synthesis" >&2
	$TILT_SYNTHESIS tmp/kdt_001.tilt -o tmp/kdt_001.tilt.f0 -otype esps -event_names "a afb"
	$CH_TRACK -info tmp/kdt_001.tilt.f0
	echo
	
}

echo >$OUTPUT

echo Tilt Test no Longer run >>$OUTPUT

#tilt_test 2>&1 >> $OUTPUT

exit 0
