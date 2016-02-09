#! /bin/sh -eu
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
 #                                                                         #
 #                   Author: Richard caley (rjc@cstr.ed.ac.uk)             #
 # ---------------------------------------------------------------------   #
 # Simple script which runs a shell for the user with some environment     #
 # set up so they can be sure to be able to do the example commands from   #
 # the documentation.                                                      #
 #                                                                         #
 ###########################################################################


useage()
{
cat <<EOF

USEAGE:
    est_examples [SHELL] [ARGS]

	Runs a new command interpreter with various things set up
	to ensure that the examples from the speech tools documentation
	will work for you.

	SHELL	- Which command interpreter you prefer. If not given your
		  normal shell will be run.

	ARGS	- Arguments given to SHELL.

EOF
    exit $1
}

prepend() {
	var="$1"
	extra="$2"
	eval "val=\$$var"
	
	if [ -n "$val" ]
		then
		val="$extra:$val"
	else
		val="$extra"
	fi
	eval "$var='$val'"
	eval "export $var"
	}

#__SHARED_SETUP__

UNDERLINE="sed -e '{p;s/./=/g;}'"
shell=${SHELL-/bin/sh}

while [ $# -gt 0 ]
    do
    case "$1" in 
    -h* | '-?' ) useage 0;;
    * ) break;;
    esac
done

if [ $# -gt 0 ]
    then
    shell=$1
    shift
fi

# Here is the environment we want.

export EST_DATA

EST_DATA='__EST__/testsuite/data'

prepend PATH '__EST__/bin'

export PS1 PS2 

PS1="est> "
PS2="est> "

cat <<EOF 


`echo Running $shell.|eval $UNDERLINE`

               PATH = $PATH
    LD_LIBRARY_PATH = $LD_LIBRARY_PATH
           EST_DATA = $EST_DATA

EOF

if [ $# -gt 0 ]
    then
    eval $shell "$@"
else
    eval $shell
fi

cat <<EOF


`echo est_examples finished.|eval $UNDERLINE`

EOF
