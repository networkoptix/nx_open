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

CH_WAVE=$TOP/bin/ch_wave
BCAT=$TOP/bin/bcat

test_conversion () {
	type=$1

	echo "$type " >&2
	
	/bin/rm -f tmp/ch_wave.wav tmp/ch_wave.nist

	$CH_WAVE -otype $type "$DATA"/ch_wave.wav -o tmp/ch_wave.wav || exit 1
	$CH_WAVE -obo MSB -otype nist tmp/ch_wave.wav -o tmp/ch_wave.nist || exit 1

        if cmp "$DATA"/ch_wave.wav tmp/ch_wave.nist 
		then echo ch_wave nist to $type to nist: pass
		else echo ch_wave nist to $type to nist: fail
	fi
}

test_raw () {
   echo raw >&2
   SAMPLE_RATE=`$CH_WAVE -info "$DATA"/ch_wave.wav | awk '{if ($1 == "Sample") print $3}'`
   $CH_WAVE -otype raw "$DATA"/ch_wave.wav -o tmp/ch_wave.raw
   $CH_WAVE -itype raw -istype short -f $SAMPLE_RATE tmp/ch_wave.raw -otype nist -ostype short -obo MSB -o tmp/ch_wave.nist
   if cmp "$DATA"/ch_wave.wav tmp/ch_wave.nist 
      then echo ch_wave raw binary test: pass
      else echo ch_wave raw binary test: fail
   fi
   $CH_WAVE -otype raw -ostype ascii "$DATA"/ch_wave.wav -o tmp/ch_wave.raw
   $CH_WAVE -itype raw -istype ascii -f $SAMPLE_RATE tmp/ch_wave.raw -otype nist -obo MSB -o tmp/ch_wave.nist
   if cmp "$DATA"/ch_wave.wav tmp/ch_wave.nist 
      then echo ch_wave raw ascii test: pass
      else echo ch_wave raw ascii test: fail
   fi

}

test_byte_order ()
{
   # Test byte order
   echo byte order >&2
   SAMPLE_RATE=`$CH_WAVE -info "$DATA"/ch_wave.wav | awk '{if ($1 == "Sample") print $3}'`
   $CH_WAVE -obo MSB -otype raw "$DATA"/ch_wave.wav -o tmp/ch_wave.raw  || exit 1
   dd if=tmp/ch_wave.raw conv=swab of=tmp/ch_wave.swab
   $CH_WAVE -ibo LSB -itype raw -istype short -f $SAMPLE_RATE tmp/ch_wave.swab -obo MSB -otype nist -o tmp/ch_wave.raw  || exit 1
   if cmp "$DATA"/ch_wave.wav tmp/ch_wave.nist 
      then echo ch_wave byte order test: pass
      else echo ch_wave byte order test: fail
   fi
}

test_stdio ()
{
   echo stdio >&2
   $CH_WAVE -otype snd "$DATA"/ch_wave.wav | $CH_WAVE - -obo MSB -otype nist -o tmp/ch_wave.nist
   if cmp "$DATA"/ch_wave.wav tmp/ch_wave.nist 
      then echo ch_wave stdio test1: pass
      else echo ch_wave stdio test1: fail
   fi
   $CH_WAVE -otype riff "$DATA"/ch_wave.wav | $CH_WAVE -obo MSB -otype nist -o tmp/ch_wave.nist
   if cmp "$DATA"/ch_wave.wav tmp/ch_wave.nist 
      then echo ch_wave stdio test2: pass
      else echo ch_wave stdio test2: fail
   fi
   $CH_WAVE -otype nist -obo nonnative "$DATA"/ch_wave.wav -o - | $CH_WAVE -obo MSB -otype nist -o tmp/ch_wave.nist
   if cmp "$DATA"/ch_wave.wav tmp/ch_wave.nist 
      then echo ch_wave stdio test3: pass
      else echo ch_wave stdio test3: fail
   fi
   $CH_WAVE -otype esps -obo nonnative "$DATA"/ch_wave.wav -o - >tmp/ch_wave.esps
   cat tmp/ch_wave.esps | $CH_WAVE - -obo MSB -otype nist -o tmp/ch_wave.nist
   if cmp "$DATA"/ch_wave.wav tmp/ch_wave.nist 
      then echo ch_wave stdio test4: pass
      else echo ch_wave stdio test4: fail
   fi
}

test_subwaves ()
{
  # cut the file up and put it back together again
  echo subrange >&2
   SAMPLE_RATE=`$CH_WAVE -info "$DATA"/ch_wave.wav | awk '{if ($1 == "Sample") print $3}'`
  $CH_WAVE -otype raw -from 0 -to 8073 "$DATA"/ch_wave.wav -o tmp/ch_wave.raw.1
  $CH_WAVE -otype raw -from 8073 -to 16146 "$DATA"/ch_wave.wav -o tmp/ch_wave.raw.2
  $BCAT tmp/ch_wave.raw.1 tmp/ch_wave.raw.2 -o tmp/ch_wave.cat
  $CH_WAVE tmp/ch_wave.cat -itype raw -istype short -f $SAMPLE_RATE -obo MSB -otype nist -o tmp/ch_wave.nist
  if cmp "$DATA"/ch_wave.wav tmp/ch_wave.nist 
     then echo ch_wave subwave test1: pass
     else echo ch_wave subwave test1: fail
  fi
  $CH_WAVE -otype raw -from 0 -to 10 "$DATA"/ch_wave.wav -o tmp/ch_wave.raw.1
  $CH_WAVE -otype raw -from 10 -to 8073 "$DATA"/ch_wave.wav -o tmp/ch_wave.raw.2
  $CH_WAVE -otype raw -from 8073 -to 16146 "$DATA"/ch_wave.wav -o tmp/ch_wave.raw.3
  $BCAT tmp/ch_wave.raw.1 tmp/ch_wave.raw.2 tmp/ch_wave.raw.3 -o tmp/ch_wave.cat
  $CH_WAVE tmp/ch_wave.cat -itype raw -istype short -f $SAMPLE_RATE -obo MSB -otype nist -o tmp/ch_wave.nist
  if cmp "$DATA"/ch_wave.wav tmp/ch_wave.nist 
     then echo ch_wave subwave test2: pass
     else echo ch_wave subwave test2: fail
  fi
}

test_channels ()
{
  # cut the file up and put it back together again
  echo channels >&2
   SAMPLE_RATE=`$CH_WAVE -info "$DATA"/ch_wave.wav | awk '{if ($1 == "Sample") print $3}'`
  $CH_WAVE -from 10 -to 8073 "$DATA"/ch_wave.wav -otype nist -o tmp/ch_wave.t1
  $CH_WAVE -from 8073 -to 8083 "$DATA"/ch_wave.wav -otype nist -o tmp/ch_wave.t2
  $CH_WAVE tmp/ch_wave.t2 tmp/ch_wave.t1 "$DATA"/ch_wave.wav -otype nist -o tmp/ch_wave.nist
  $CH_WAVE -from 10 -to 8073 tmp/ch_wave.nist -otype nist -o tmp/ch_wave.t2
  if cmp tmp/ch_wave.t1 tmp/ch_wave.t2
     then echo ch_wave concat : pass
     else echo ch_wave concat : fail
  fi

  $CH_WAVE -pc longest "$DATA"/ch_wave.wav tmp/ch_wave.nist -o tmp/ch_wave.tc1
  $CH_WAVE -from 10 -to 8073 tmp/ch_wave.tc1 -otype nist -o tmp/ch_wave.tc2
  $CH_WAVE -c 0 tmp/ch_wave.tc2 -otype nist -o tmp/ch_wave.tc3
  $CH_WAVE -from 10 -to 8073 "$DATA"/ch_wave.wav -otype nist -o tmp/ch_wave.tc4
  if cmp tmp/ch_wave.tc4 tmp/ch_wave.tc3
     then echo ch_wave channel combine/extract : pass
     else echo ch_wave channel combine/extract : fail
  fi
}

test_defft ()
{
  echo default file type >&2
  # the following shouldn't complain
  $CH_WAVE -lpfilter 3000 -forder 19 "$DATA"/ch_wave.wav -o tmp/ch_wave.wav
  $CH_WAVE "$DATA"/ch_wave.wav -obo MSB -o tmp/ch_wave.wav
  if cmp "$DATA"/ch_wave.wav tmp/ch_wave.wav
     then echo ch_wave default file type: pass
     else echo ch_wave default file type: fail
  fi

}

test_keylab()
{
  echo keylab divide and extract >&2
  (cd tmp; ../$CH_WAVE -divide -key "$DATA"/key.lab "$DATA"/ch_wave.wav -otype nist -ext .wav)
  $CH_WAVE -info tmp/w*.wav
  (cd tmp; ../$CH_WAVE -extract w2 -key  "$DATA"/key.lab "$DATA"/ch_wave.wav -otype nist -o w2.w2)
  if cmp tmp/w2.wav tmp/w2.w2
     then echo ch_wave key extract : pass
     else echo ch_wave ket extract : fail
  fi
}

test_info ()
{
  echo info and help >&2
  $CH_WAVE -info "$DATA"/ch_wave.wav
  $CH_WAVE -h 
  $CH_WAVE -F 20000 "$DATA"/ch_wave.wav -o tmp/ch_wave.nist
  $CH_WAVE -F 8000 tmp/ch_wave.nist -o tmp/ch_wave.nist8
  $CH_WAVE -info "$DATA"/ch_wave.wav tmp/ch_wave.nist tmp/ch_wave.nist8
}

echo >$OUTPUT

test_conversion esps 2>&1 >> $OUTPUT
test_conversion snd 2>&1 >> $OUTPUT
test_conversion riff 2>&1 >> $OUTPUT
test_conversion audlab 2>&1 >> $OUTPUT
test_conversion aiff 2>&1 >> $OUTPUT
test_conversion est 2>&1 >> $OUTPUT
test_raw 2>&1 >> $OUTPUT

test_byte_order 2>&1 >> $OUTPUT
test_stdio 2>&1 >> $OUTPUT
test_subwaves 2>&1 >> $OUTPUT
test_channels 2>&1 >> $OUTPUT
test_defft 2>&1 >> $OUTPUT
test_keylab 2>&1 >> $OUTPUT
test_info 2>&1 >> $OUTPUT

exit 0
