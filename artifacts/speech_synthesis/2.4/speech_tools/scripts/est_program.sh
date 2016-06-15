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
 # Script which creates or modifies a makefile for use with                #
 # the speech_tools library.                                               #
 #                                                                         #
 ###########################################################################



useage()
{
cat <<EOF

USEAGE:
    est_program name

	Creates a 'Makefile' in the current directory which will build
	a program 'name' from 'name.cc', giving access to the Edinburgh
	Speech Tools library. If 'name.cc' doesn't exist it is created
	containing a simple main function.  
        
EOF
    exit $1
}

check_makefile()
{
    ok=`awk '/EST_HOME=/ {home=1} /## PROGRAMS/ {programs=1} /## RULES/ {rules=1} END {print (home&&programs&&rules)?"true":"false";}' Makefile`

    if $ok
	then
	echo "	Makefile OK"
    else
	echo "	Makefile is not an EST Makefile"
	exit 1
    fi
}

create_empty_makefile()
{
    echo "  Creating Makefile"
    cp __EST__/lib/est_program_makefile Makefile
    chmod +w Makefile
}

check_mainline()
{
    m=$1
    ok=`awk '/#include "EST.h"/ {include=1} /parse_command_line/ {cline=1} END {print (include&&cline)?"true":"false";}' $m`

    if $ok
	then
	echo "	$m OK"
    else
	echo "	$m is not an EST main program"
	exit 1
    fi
}

create_mainline()
{
    m=$1
    echo "  Creating $m"
    cp __EST__/lib/est_mainline "$m"
    chmod +w "$m"
}

ensure_entry_for()
{
    n="$1"
    awk '
	BEGIN {state=0;}

	state==0 && /^## PROGRAMS/ {state=1;}

	state==0 && /^EXECS =/ {
	    fl=0;
	    for(i=1; i<=NF; i++)
		if ($(i) == name)
		    fl=1;

	    if (!fl)
		$0 = $0 " " name;
	    }
	
	state==1 && /^## FOR / && $3 == name {state=2;}

	state==1 && /## RULES/ {
	    print "## FOR " name;
	    print "";
	    print "# C source files";
	    print name "_CSRC= ";
	    print "";
	    print "# C++ source files";
	    print name "_CXXSRC= " name ".cc ";
	    print "";
	    print "# Extra -I arguments";
	    print name "_INCLUDES=";
	    print "";
	    print "# Extra -L and -l arguments";
	    print name "_LIBS=";
	    print "";
	    print "";
	    print "# Extra -D arguments";
	    print name "_DEFINES=";
	    print "";
	    print name "_OBJ=\$(" name "_CSRC:.c=.o) \$(" name "_CXXSRC:.cc=.o)";
	    print "";
	    print name ": \$(" name "_OBJ)";
	    print "";
	    print "";
	    state=3;
	    }

	state==2 && /## RULES/ {
	    state=3;
	    }

	    {print}
	' name="$n" Makefile > Makefile.new
	cp Makefile Makefile.old
	cp Makefile.new Makefile
}

add_executable()
{
    exec_name="$1"
    main="$2"
    shift 2

    ensure_entry_for "$exec_name"
}

#__SHARED_SETUP__

UNDERLINE="sed -e '{p;s/[^ 	]/=/g;}'"

while [ $# -gt 0 ]
    do
    case "$1" in 
    -h* | '-?' ) useage 0;;
    * ) break;;
    esac
done

if [ $# -gt 1 ] 
    then
    useage 1
fi

exec_name="$1"
shift

main="$exec_name.cc"

if [ -f Makefile ]
    then
    check_makefile
else
    create_empty_makefile
fi

if [ -f "$main" ]
    then
    check_mainline "$main"
else
    create_mainline "$main"
fi

if [ -n "${*-}" ]
    then
    add_executable "$exec_name" "$main" "$@"
else
    add_executable "$exec_name" "$main"
fi

exit 0

