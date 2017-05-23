#!/bin/sh -eu

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
 ##                   Author: Richard caley (rjc@cstr.ed.ac.uk)           ##
 ##                     Date: June 1997                                   ##
 ## --------------------------------------------------------------------- ##
 ## Script to resynthesise a waveform, for example to impose a new        ##
 ## intonation contour. Requires pitchmarks for the waveform.             ##
 ##                                                                       ##
 ###########################################################################


useage()
{
cat <<EOF

    resynth [-i] [-v] WAVEFILE PMFILE [-root BASNAME] [SYNTHESIS_ARGUMENTS]

	WAVEFORM - name of waveform file.
	PMFILE   - name of pitchmark file.

	-i       - invert waveform before processing
	-v	 - detect voicing

    Remaining arguments are passed to lpc_synthesis as follows
EOF
    lpc_synthesis -help | sed -e '/^/s//            /' -e '/Useage:/d'
cat <<EOF

EOF
    exit $1
}

#__SHARED_SETUP__

invert=false
voiced=''
pre=0

while true
    do
    case "${1-}" in
    -h* ) useage 0;;
    '-?' ) useage 0;;
    '-i' ) invert=true; shift;;
    '-v' ) voiced='-voiced'; shift;;
    * ) break;;
    esac
done

if [ $# -lt 2 ]
    then
    useage 1
fi

name="$1";
pmfile="$2";

shift
shift

if [ "${1-}" = "-root" ]
    then
    shift
    root="$1"
    shift
else
    b=`basename "$name"`
    root=`expr "$b" : '\([^/.]*\).*$'`
fi

if $invert
    then
    ch_wave $name -otype nist -o "${root}_i.nist" -s -1 
    todo="${root}_i.nist"
else
    todo="$name"
fi

lpc_analysis $todo -pm $pmfile \
    -order 20 -window rectangle \
    -length 0.01 -shift 0.01 -preemph $pre\
    -otype esps -o "${root}_lpcs.esps" \
    -rtype nist -r "${root}_res.nist"  \
    -power $voiced \
    -pm_adjust_to_peak -pm_at_trans 30

lpc_synthesis "${root}_lpcs.esps" -r "${root}_res.nist" \
    -otype nist -o "${root}_rs.nist" -preemph $pre \
    -pp_mod_func stretch_1_3 -pp_mod_func_u chop_1_3 \
    "${@-}"


