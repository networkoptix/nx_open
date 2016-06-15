#!/bin/sh
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
 ## Look for module definitions.                                          ##
 ##                                                                       ##
 ###########################################################################

if [ -f $TOP/config/modules/descriptions ]
    then
    . $TOP/config/modules/descriptions
fi

if [ -z "$MODULE_DIRECTORY" ]
    then
    MODULE_DIRECTORY="$TOP/"
fi

MOD_DIR_TRIMMED=`expr "$MODULE_DIRECTORY" : "$TOP/\(.*\)$"`

if [ -z "$MOD_DIR_TRIMMED" ]
    then
    MOD_DIR_TRIMMED="."
fi

echo ' ###########################################################################'
echo ' ## This file is created automatically from your config file.'
echo ' ## Do not hand edit.'
echo ' ## Created:'`date`
echo ' ###########################################################################'

echo ''

for M in $*
	do
	echo "	$M" >&2
	m=`echo "$M" | tr ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz`
	fl=false

        eval "listed_desc=\"\$desc_$m\""
	if [ -n "$listed_desc" ]
	    then
	    echo "MOD_DESC_${M}=$listed_desc"
	    echo "INCLUDE_${M}=1"
	    if [ -d "$MODULE_DIRECTORY/$M" ]
		then
		echo "ifeq (\$(DIRNAME),$MOD_DIR_TRIMMED)"
		echo '    EXTRA_LIB_BUILD_DIRS := $(EXTRA_LIB_BUILD_DIRS) ' $M
		echo "endif"
		f="$MODULE_DIRECTORY/$M"
	    elif [ -d "$MODULE_DIRECTORY/$m" ]
		then
		echo "ifeq (\$(DIRNAME),$MOD_DIR_TRIMMED)"
		echo '    EXTRA_LIB_BUILD_DIRS := $(EXTRA_LIB_BUILD_DIRS) ' $m
		echo "endif"
		f="$MODULE_DIRECTORY/$m"
	    else
	        f="ok"
	    fi
	    fl=true
	fi

	$fl || for f in $TOP/config/modules/$M.mak \
	    $TOP/config/modules/$m.mak \
	    $MODULE_DIRECTORY/$M/$M.mak \
	    $MODULE_DIRECTORY/$M/$m.mak \
	    $MODULE_DIRECTORY/$m/$M.mak \
	    $MODULE_DIRECTORY/$m/$m.mak
		do
		if [ -f "$f" ]
			then
			f1=`expr "$f" : "$TOP/\(.*\)$"`
			echo "include \$(TOP)/$f1"
			fl=true
			f=$f1
			break
		fi
	done

	$fl || for f in \
	    `find $MODULE_DIRECTORY \( -name CVS -prune \) -o \( -name "$M.mak" -o -name "$m.mak" \) -print -follow`
		do
		if [ -f "$f" ]
			then
			f1=`expr "$f" : "$TOP/\(.*\)$"`
			echo "include \$(TOP)/$f1"
			fl=true
			f=$f1
			break
		fi
	done

	$fl || for n in $M $m
	    do
	    if [ -d "$MODULE_DIRECTORY/$n" ]
		then
		[ -n "$listed_desc" ] || { 
		    echo "MOD_DESC_${M}=Unknown Module"
		    echo "INCLUDE_${M}=1"
		    }
		echo "ifeq (\$(DIRNAME),$MOD_DIR_TRIMMED)"
		echo '    EXTRA_LIB_BUILD_DIRS := $(EXTRA_LIB_BUILD_DIRS) ' $n
		echo "endif"
		fl=true
		f="unknown module in $MOD_DIR_TRIMMED/$n"
		break;
	    fi
	done


	if $fl 
	    then 
		echo "		$f" >&2
	else 
	    echo "		NOT FOUND" >&2
	    echo "MOD_DESC_${M}='!! MISSING MODULE !!'"
	    echo  "ifndef MADE_FROM_ABOVE"
	    echo "ifneq (\$(shell echo No definition for module $m >&2),xxx)"
	    echo "foo:bar"; echo "endif"
	    echo endif
	fi
done

