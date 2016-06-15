#!/bin/sh
###########################################################################
##                                                                       ##
##                                                                       ##
##              Carnegie Mellon University, Pittsburgh, PA               ##
##                      Copyright (c) 2007-2010                          ##
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
##  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK         ##
##  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      ##
##  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   ##
##  SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE      ##
##  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    ##
##  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   ##
##  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          ##
##  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       ##
##  THIS SOFTWARE.                                                       ##
##                                                                       ##
###########################################################################
##                                                                       ##
##          Author :  S P Kishore (skishore@cs.cmu.edu)                  ##
##          Date   :  February 2007                                      ##
##                                                                       ##
###########################################################################


islicedir=$FESTVOXDIR/src/interslice
rawF=etc/raw.done.data
prompF=etc/txt.done.data
phseqF=etc/txt.phseq.data
phseqIF=etc/txt.phseq.data.int
phlist=etc/ph_list.int
bwav=bwav/bwav.wav
logF=etc/tlog.txt

if [ $1 = "help" ]
then 
  echo 'Splits long files with long text into utterance sized waveforms with'
  echo 'the appropriate words ready for further phoneme alignment'
  echo '   $FESTVOXDIR/src/interslice/scripts/do_islice_v2.sh setup'
  echo '  ./bin/do_islice_v2 islice BIGFILE.txt BIGFILE.wav'
  echo 'Will generate BIGFILE.data and wav/*.wav split waveforms.'
  echo '  Manual copy'
  echo '  copy / link large audio file to bwav/bwav.wav'
  echo '  copy the text onto etc/raw.done.data'
  echo 'sh bin/do_islice_v2.sh getprompts [prefix]'
  echo 'sh bin/do_islice_v2.sh phseq'
  echo 'sh bin/do_islice_v2.sh phseqint'
  echo 'sh bin/do_islice_v2.sh doseg'
  echo 'sh bin/do_islice_v2.sh slice'
fi

if [ $1 = "setup" ]
then
   mkdir etc
   mkdir tmp
   mkdir feat
   mkdir lab
   mkdir bwav
   mkdir twav

   cp -p $FESTVOXDIR/src/interslice/model/ph_list.int etc/
   cp -p $FESTVOXDIR/src/interslice/model/model101.txt etc/
   cp -p $FESTVOXDIR/src/interslice/model/global_mn_vr.txt etc/

   cp -r $islicedir/bin ./
   cp -p $islicedir/scripts/do_islice_v2.sh ./bin/do_islice_v2.sh
   cp -p $FESTVOXDIR/src/ehmm/bin/FeatureExtraction bin/FeatureExtraction.exe
   cp -r $islicedir/scripts ./

   echo Ready to slice ... use
   echo "   ./bin/do_islice_v2 islice BIGFILE.txt BIGFILE.wav"

  exit 
fi

if [ $1 = "islice" ]
then
   dbname=`basename $2 .txt`
   tfiledirname=`dirname $2`

   echo Generating prompts from $2 text file
   $0 getprompts $2 $dbname $tfiledirname/$dbname.data

   $ESTDIR/bin/ch_wave -F 16000 $3 -o bwav/bwav.wav || exit -1
   cp -pr $tfiledirname/$dbname.data etc/txt.done.data

   $0 phseq
   $0 phseqint
   $0 doseg
   $0 slice

   exit
fi

if [ $1 = "getprompts" ]
then

   $FESTVOXDIR/src/promptselect/text2utts -all $2 -dbname $3 -o $4

   exit
fi

if [ $1 = "phseq" ]
then
   perl scripts/phseq.pl $prompF $phseqF
   exit
fi

if [ $1 = "phseqint" ]
then
   perl scripts/prmp2int.pl $phseqF $phlist
   exit
fi

if [ $1 = "doseg" ]
then
   perl scripts/do_seg.pl $phseqIF $phlist $bwav
   exit
fi

if [ $1 = "slice" ]
then
   perl scripts/slice_wav.pl $logF twav $bwav
   exit
fi

echo unknown option $*
exit


exit 1;

