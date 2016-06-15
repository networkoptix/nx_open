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
SIG2FV=$TOP/bin/sig2fv
SIGFILTER=$TOP/bin/sigfilter
CH_TRACK=$TOP/bin/ch_track
CH_WAVE=$TOP/bin/ch_wave

DATA=$TOP/lib/example_data

lpc_test () {

	/bin/rm -f tmp/kdt_001.lpc
	/bin/rm -f tmp/kdt_001.res

	echo "LPC params" >&2
	echo "LPC params"
	$SIG2FV  "$DATA/kdt_001.wav" -o tmp/kdt_001.lpc -otype est -lpc_order 16 -coefs "lpc" -pm "$DATA/kdt_001.pm" -preemph 0.95 -factor 3 -window_type hamming 
	$SIGFILTER "$DATA/kdt_001.wav" -o tmp/kdt_001.res -otype nist -lpcfilter tmp/kdt_001.lpc -inv_filter
	$CH_TRACK -info tmp/kdt_001.lpc
	$CH_WAVE -info tmp/kdt_001.res
	# Should not be any of these unless there is an error
	grep "0.000000" tmp/kdt_001.lpc
	grep "NaN" tmp/kdt_001.lpc
	grep -i "Infinity" tmp/kdt_001.lpc
	
}

mfcc_test () {

	/bin/rm -f tmp/kdt_001.mfcc

	echo "MFCC params" >&2
	echo "MFCC params"
	$SIG2FV  -coefs melcep  -delta melcep -melcep_order 12 -fbank_order 24 -shift 0.005 -factor 5.0 -preemph 0.97 -otype est "$DATA/kdt_001.wav" -o tmp/kdt_001.mfcc
	$CH_TRACK -info tmp/kdt_001.mfcc
        echo "expect one line containing 0s (first delta params)"
	numzeros=`grep "0.000000" tmp/kdt_001.mfcc | wc -l`
	echo "Number of vectors with 0s is " $numzeros
}

echo >$OUTPUT

lpc_test 2>&1 >> $OUTPUT
mfcc_test 2>&1 >> $OUTPUT

exit 0
