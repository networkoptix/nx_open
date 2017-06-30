#! /bin/csh -f
#######################################################################
##                                                                   ##
##            Nagoya Institute of Technology, Aichi, Japan,          ##
##       Nara Institute of Science and Technology, Nara, Japan       ##
##                                and                                ##
##             Carnegie Mellon University, Pittsburgh, PA            ##
##                      Copyright (c) 2003-2004                      ##
##                        All Rights Reserved.                       ##
##                                                                   ##
##  Permission is hereby granted, free of charge, to use and         ##
##  distribute this software and its documentation without           ##
##  restriction, including without limitation the rights to use,     ##
##  copy, modify, merge, publish, distribute, sublicense, and/or     ##
##  sell copies of this work, and to permit persons to whom this     ##
##  work is furnished to do so, subject to the following conditions: ##
##                                                                   ##
##    1. The code must retain the above copyright notice, this list  ##
##       of conditions and the following disclaimer.                 ##
##    2. Any modifications must be clearly marked as such.           ##
##    3. Original authors' names are not deleted.                    ##
##                                                                   ##    
##  NAGOYA INSTITUTE OF TECHNOLOGY, NARA INSTITUTE OF SCIENCE AND    ##
##  TECHNOLOGY, CARNEGIE MELLON UNiVERSITY, AND THE CONTRIBUTORS TO  ##
##  THIS WORK DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,  ##
##  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, ##
##  IN NO EVENT SHALL NAGOYA INSTITUTE OF TECHNOLOGY, NARA           ##
##  INSTITUTE OF SCIENCE AND TECHNOLOGY, CARNEGIE MELLON UNIVERSITY, ##
##  NOR THE CONTRIBUTORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR      ##
##  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM   ##
##  LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,  ##
##  NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN        ##
##  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.         ##
##                                                                   ##
#######################################################################
##                                                                   ##
##          Author :  Tomoki Toda (tomoki@ics.nitech.ac.jp)          ##
##          Date   :  June 2004                                      ##
##                                                                   ##
#######################################################################
##                                                                   ##
##  Estimating GMM Parameters with EM Algorithm                      ##
##                                                                   ##
#######################################################################
 
set src_dir = $argv[1]	# src directory
set clsnum = $argv[2]	# the number of classes
set jnt = $argv[3]	# joint features
set cblbl = $argv[4]	# codebook file label
set initlbl = $argv[5]	# initial parameter file label
set gmmlbl = $argv[6]	# GMM parameter file label

##############################
set gmm = $src_dir/gmm/gmm_jde
set gmm_para = $src_dir/gmmpara/gmm_para
##############################
set order = 24
@ dim = $order + $order
@ jdim = $dim + $dim
##############################

if (!(-r $gmmlbl$clsnum.param)) then
	if (-r $gmmlbl$clsnum.res) then
		rm -f $gmmlbl$clsnum.res
	endif
	$gmm \
		-flcoef 0.001 \
		-dim $jdim \
		-wghtfile $initlbl$clsnum.wght \
		-meanfile $cblbl$clsnum.mat \
		-covfile $initlbl$clsnum.cov \
		-bcovfile $initlbl"1.cov" \
		-dia \
		$jnt \
		$gmmlbl$clsnum.wght \
		$gmmlbl$clsnum.mean \
		$gmmlbl$clsnum.cov \
		> $gmmlbl$clsnum.res

	$gmm_para \
		-nmsg \
		-dim $jdim \
		-ydim $dim \
		-dia \
		$gmmlbl$clsnum.wght \
		$gmmlbl$clsnum.mean \
		$gmmlbl$clsnum.cov \
		$gmmlbl$clsnum.param

	rm -f $gmmlbl$clsnum.wght
	rm -f $gmmlbl$clsnum.mean
	rm -f $gmmlbl$clsnum.cov
endif
if (!(-r $gmmlbl$clsnum.param)) then
	echo "Error: Can't estimate GMM parameters."
	echo "Error: Probably the number of classes is too large."
	echo ERROR > DUMMY-ERROR-LOG
endif
