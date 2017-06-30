#!/bin/sh
###########################################################################
##                                                                       ##
##                Centre for Speech Technology Research                  ##
##                     University of Edinburgh, UK                       ##
##                         Copyright (c) 1997                            ##
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

CH_TRACK=$TOP/bin/ch_track
DATA=../lib/example_data

set -x

test_conversions () {

	echo conversion >&2
	
	/bin/rm -f tmp/ch_track.htk tmp/ch_track.track

	# Get htk version with basic header
	$CH_TRACK -otype htk "$DATA"/ch_track.htk -o tmp/ch_track.htk || exit 1

	$CH_TRACK -otype esps tmp/ch_track.htk -o tmp/ch_track.track || exit 1
	$CH_TRACK -otype htk tmp/ch_track.track -o tmp/ch_track.htk2 || exit 1
        if cmp tmp/ch_track.htk tmp/ch_track.htk2
		then echo ch_track htk to esps to nist: pass
		else echo ch_track htk to esps to nist: fail
	fi

	$CH_TRACK -otype ascii tmp/ch_track.htk -o tmp/ch_track.track || exit 1
	$CH_TRACK -itype ascii -s 0.010 -otype htk_fbank tmp/ch_track.track -o tmp/ch_track.htk2 || exit 1
	$CH_TRACK -otype ascii tmp/ch_track.htk2 -o tmp/ch_track.ascii || exit 1
        if cmp tmp/ch_track.track tmp/ch_track.ascii
		then echo ch_track htk to ascii to htk: pass
		else echo ch_track htk to ascii to htk: fail
	fi

	$CH_TRACK -otype htk "$DATA"/ch_track.htk -o tmp/ch_track.htk || exit 1

	$CH_TRACK -otype htk_user tmp/ch_track.htk -o tmp/ch_track.track || exit 1
	$CH_TRACK -otype htk tmp/ch_track.track -o tmp/ch_track.htk2 || exit 1
        if cmp tmp/ch_track.htk tmp/ch_track.htk2
		then echo ch_track htk to htk_user to htk: pass
		else echo ch_track htk to htk_user to htk: fail
	fi
}

test_info ()
{
  echo info and help >&2
  $CH_TRACK -info "$DATA"/ch_track.htk
  $CH_TRACK -h 
  $CH_TRACK "$DATA"/ch_track.htk -otype esps -o tmp/ch_track.esps
  $CH_TRACK -info "$DATA"/ch_track.htk tmp/ch_track.esps
}

echo >$OUTPUT

test_conversions 2>&1 >> $OUTPUT
test_info 2>&1 >> $OUTPUT

exit 0
