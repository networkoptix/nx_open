Executable Programs {#estexec}
===================

## Manuals

See @subpage estmanuals for more information

# Building your own executable programs {#estexecbuilding}

A simple mechanism is provided for doing all the configuration needed
to build a executable program which uses the speech tools library.

First, make a directory which will hold your program. To make a program called "do_stuff", type

    est_program do_stuff

if you haven't got the EST bin directory in your path you will have
to add the path explicitly, e.g.

    speech_tools/bin/est_program do_stuff


This will create a Makefile and a .cc file called
*do_stuff_main.cc*, which will look something like
      this:

~~~~~~~~~~~~~~~{.cc}
#include "EST.h"
#include "EST_types.h"
#include "EST_error.h"

int main(int argc, char *argv[]) 
{ 
    EST_StrList files; // the list of input files will go here
    EST_Option cmd_line; // the parsed list of command line arguments
	                 // will go here.

	  // This bit parses the command line args and puts them into
	  // files and cmd_line
    parse_command_line
        (argc, argv, 
         EST_String("[OPTIONS] [files...]\n")+
         "Summary; DO SOMETHING\n"+
         "-o [ofile]       Ouptut file\n",
         files, cmd_line);

    EST_String out_file; // the name of the output file

    // If a output file has been specified using -o, put it in out_file
    if (cmd_line.present("-o"))
      out_file = cmd_line.val("-o");
    else
      EST_error("No output file specified");

   // declare EST_StrList iterator
    EST_StrList::Entries fs;

    // iterate through files and do something.
    for(fs.begin(files); fs; ++fs)
      {
        EST_String file = *fs;

        // Process file
      }

    return 0;
}
~~~~~~~~~~~~~~~

You can now add any C++ code to this, and compile by typing *make*.

If you want to create a second program in the same directory, type the
same again:

    speech_tools/bin/est_program do_more_stuff

This time, *do_more_stuff_main.cc* will be created and the
appropriate build commands added to the extisting Makefile. If you
wish to add an extra .cc file to particular program, simply edit the
Makefile and add it on the line:

    do_stuff_CXXSRC= do_stuff.cc extra.cc



