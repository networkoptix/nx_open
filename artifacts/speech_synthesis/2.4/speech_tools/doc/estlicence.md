Licence, Installation and acknowledgements      {#estlicence}
=====================================

[TOC]

# Licence  {#licence}
Since version 1.2.0 we are distributing the Edinburgh Speech Tools
under a free software lince similar to the X11 one.  Basically the
system is free for anyone to use commercially or otherwise
without further permission.


Hence the current copy policy is

                    Centre for Speech Technology Research                  
                         University of Edinburgh, UK                       
                           Copyright (c) 1994-1999                            
                            All Rights Reserved.                           
                                                                           
      Permission is hereby granted, free of charge, to use and distribute  
      this software and its documentation without restriction, including   
      without limitation the rights to use, copy, modify, merge, publish,  
      distribute, sublicense, and/or sell copies of this work, and to      
      permit persons to whom this work is furnished to do so, subject to   
      the following conditions:                                            
       1. The code must retain the above copyright notice, this list of    
          conditions and the following disclaimer.                         
       2. Any modifications must be clearly marked as such.                
       3. Original authors' names are not deleted.                         
       4. The authors' names are not used to endorse or promote products   
          derived from this software without specific prior written        
          permission.                                                      
                                                                           
      THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        
      DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      
      ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   
      SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     
      FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    
      WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   
      AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          
      ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       
      THIS SOFTWARE.                                                       
    

# Acknowledgments {#estacknowledgements}

## Copyright

  - 1994-1999,  Centre for Speech Technology, University of Edinburgh

## Authors

  - [Paul Taylor](http://www.cstr.ed.ac.uk/~pault)
  - [Richard Caley](http://www.cstr.ed.ac.uk/~rjc)
  - [Alan W. Black](http://www.cstr.ed.ac.uk/~awb)
  - [Simon King](http://www.cstr.ed.ac.uk/~simonk)


During 1994-1999, the above people were supported by the UK Physical
Science and Engineering Research Council though grants GR/J55106,
GR/K54229, GR/L53250, by Sun microsystems, AT&T and Reuters.

### Other contributors

We are extremely grateful to the following people who have made their
code available for general use. We should make it clear that these
people did not directly participate in the development of the library
and hence cannot be held responsible for any problems we have introduced
in the use of their code.

 - **Markus Mummer**: Waveform re-sampling routine (rateconv)
 - **Tony Robinson**: Provided cepstral and LPC routines
 - **Richard Tobin and LTG**: RXP, an XML parser.
 - **Paul Bagshaw**: The pitch tracker
 - **Craig Reese and John Campbell**: ulaw conversion routines
 - **Paradigm Associates and George Carrett**: Scheme in one defun.
 - **Theo Veenker (Utrecht University)**: IRIX audio support.
 - **Rick Woudenberg**: Inspiration for the design of the wave class
 - **Borja Etxebarria**: LPC reflection coefficients
 - **Simmule Turner and Rich Salz**: editline command line editing library
 - **Henry Spencer**: for his regex code taken from the FreeBSD 2.1.5 distribution.
 - **The Regents of the University of California**: for some string comparison code.


# What is new  {#estwhatsnew}

Since the last public release, 1.1.1, January 1999, there have been
a number of enhancements to the library system

## Free licence

To make the system more useful for more people and place
it properly in the free software community the system is now free
for both commercial and non-commercial use alike.

## Utterance consolidation

A number of tidy ups have been performed on the
`EST_Utterance` and related classes.  This makes
the interface cleaner and should have littel affect on existsing code.

## Generalization of Features

Any new object can become a feature value after simple registration
of the object through a provided macro.  We also make much
more use of this.

## SIOD tidy up

Scheme is no longer "in one defun" but "in one directory".  The
system was split out into separate files.  New objects are now
held as `EST_Val` and it is easier to add new
objects to the system.

## C++ class documentation

We now include documentation on a per class basis using 
doxygen and markdown for more general
descriptions and examples.  This has improved the coverage and quality
of low level documentation for all classes in the system.

## Tilt Analysis/Resynthesis

For intonation analysis the tilt code has been completely rewritten.



# Installation            {#estinstall}

## Release notes {#estinstallrnotes}

While previous versions of the speech tools were primarily released solely
to support the Festival Speech Synthesis System, the Edinburgh Speech
Tools Library now contains sufficiently useful tools that it is of use
in its own right.

Although hope that the speech tools has stabilised to a certain extent
and less structural changes will occur in future versions we don't
guarantee future compatibility, although every effort will be made to
make upgrading as easy as possible.  In addition, we warn that while
several programs and routines are quite mature, others are young and
have not be rigorously tested. Please do not assume these programs work.

## Requirements {#estinstallreq}

In order to compile and install the Edinburgh Speech Tools you 
need the following

### GNU Make

 Any recent version, the various `make`
 programs that come with different UNIX systems are wildly
 varying and hence it makes it too difficult to write
 Makefiles which are portable, so we
 depend on a version of `make` which is
 available for all of the platforms we are aiming at.

### A C++ compiler

The system was developed primarily with GNU C++ version 2.7.2, but
we also have compiled it successfully with a number of
other versions of gcc, Sun CC and Visual C.

Hopefully we have now sanitized the code sufficiently to to make it
possible for ports to other C++ compilers without too much difficulty.
But please note C++ is not a fully standardized language and each
compiler follows the incomplete standard to various degrees.  Often
there are many but simple problems when porting to new C++ compilers.
We are trying to deal with this by increasing our support.  However, it
is likely that small changes will be required for C++ compilers we have
not yet tested the system under.

However we feel this is stable enough to make it worthwhile attempting
ports to other C++ compilers that we haven't tried yet.

Before installing the speech tools it is worth ensuring you have a fully
installed and working version of your C++ compiler.  Most of the
problems people have had in installing the speech tools have been due to
incomplete or bad compiler installation.  It might be worth checking if
the following program works, if you don't know if anyone has used your
C++ installation before.

    #include <iostream>
    int main (int argc, char **argv)
    {
       std::cout << "Hello world\n";
    }


### Supported Systems

We have successfully compiled and tested the speech tools on the
following systems, except where specified we include support for
both shared and static versions of the libraries:

 - *Sun Sparc Solaris 2.5.1/2.6/2.7*: GCC 2.7.2, GCC 2.8.1, SunCC 4.1, egcs 1.1.1, egcs 1.1.2
 - *Sun Sparc SunOS 4.1.3*: GCC 2.7.2 (static only)
 - *Intel Solaris 2.5.1*: GCC 2.7.2
 - *FreeBSD for Intel 2.1.7, 2.2.1, 2.2.6 (aout), 2.2.8 (aout), 3.x (elf)*: GCC 2.7.2.1 (static only)
 - *Linux (2.0.30) for Intel (RedHat 4.[012]/5.[01]/6.0)*: GCC 2.7.2, GCC 2.7.2/egcs-1.0.2, egcs-1.1.2
 - *Windows NT 4.0*, *Windows95*, *Windows 98*: GCC with egcs (from [cygwin](http://www.cygwin.com/) b20.1), VisualC; 5.0. (static only)


As stated before, C++ compilers are not standard and it is non-trivial to
find the correct dialect which compiles under all.  We recommend the
use of GCC 2.7.2 if you can use it, it is the most likely one to
work. Some of the compilers listed above produce a large number of
warnings when compileing the code.

Previous versions of the system have successfully compiled under SGI
IRIX 5.3, OSF (Alphas) and HPUX but at time of writing this we have not
yet rechecked this version.

### Java Support

The *java* directory contains optional Java
classes which give some access to *speech tools* facilities from Java
programs. This has been created to support the *fringe* graphical
interface. There are two levels of support enabled by the
*JAVA* and *JAVA_CPP* options in the
`config` file. *JAVA* compiles some
very basic classes intended to allow very simple simulation of *speech tools*
facilities in pure  programs.  *JAVA_CPP*
compiles classes which use the Java native interface to provide
access to the actual *speech tools* C++ classes.

### Windows 95/98/NT Port

We have done two ports of this code to Windows machines, one uses the
*cygwin* package, which provides a Unix like environment under on
Win32 systems, the other is a native port using *VisualC*.

For our full *Windows NT* and *Windows 95/98* ports we use the Cygnus Cygwin environment (version b20.1) available from [http://cygwin.com/].
*Windows 98* is significantly more stable than *Windows 95*, especially
when many processes are started and stopped as is the case when
compiling with Cygwin. We **strongly** reccomend 98
rather than 95 be used if at all possible. However with both 95 and 98
you can expect Windows to occasionally lock up or complain it is
unable to start new processes or otherwise missbehave. You will be
restarting windows regularly. A Windows NT system should not have
these problems.

The port using Visual C does not
provide all of the features of the Unix and Cygwin versions. You
will need access to a Unix or Cygwin system to create the makefiles
used for the Visual C compilation.


Both Cygwin and Visual C ports have a number of limitations. 

#### Shared library creation is not supported
Creation of Windows *DLLs* is different
enough from creation of Unix shared libraries that the
support does not carry directly accross, and we haven't
yet had time to work on it.

#### Java not supported
Because the Java support is related to the creation of
shared libraries, this is also not yet implemented for
Windows. 

#### Command line editing limited
Because of the limiots of the Windows DOS console
	      window, the command line editing in
	      *siod* is less reliable (for instance
	      on very long lines). 

#### (Visual C) Networking not supported
Networking under Win32 is different from Unix in a
number of fairly fundamental ways, we haven't tackled
this at all.

There are no doubt other differences we have not noticed. We don't use
Windows for any of our work and so the Windows builds of our systems
don't get the extensive use the UNIX builds do.

## Building speech tools {#estinstallbuild}

### Configuration

All compile-time configuration for the system is done through the user
definable file *config/config*.  You must create this
file before you can compile the library.  An example is given in
*config/config-dist*, copy it and change its permissions
to give write access

    cd config
    cp config-dist config
    chmod +w config

In many cases no further changes will be required, but it might
be worth checking the contents of this file just in case.   Where
possible the system will automatically config itself.


Type

    gnumake info


This will create the local config files and display what it
thinks your system is.


If this is not suitable for your machine then
edit your *config/config*.  In most cases
the default will be the best option.  If you are unsure about
what you should change you probabaly shouldn't change anything.


As of 1.3.1 due to conflicts in copyright we have dropped *GNU Readline*
support and replaced it with a differently licenced alternative which
does not imposes the restrictions of the GPL.  *editline* is a free
command line editor library.  We have added substantially to it,
including support for long lines, incremental search and completion.
However as it has not yet been tested on many systems it is still
optional, though is on by default.

For Linux we now fully support shared libraries and even recommend
them.  However if you are going to do a lot of development and
don't understand the consequences of shared libraries and getting
*LD_LIBRARY_PATH* correct (or what that is) we
recommend you compile unshared, the default.  If you are going to
simply run the speech tools (and festival) then shared is a reasonable
option.  Uncomment the line in the *config/config* file:

    # SHARED = 1

Shared support under Solaris is complete for all the speech tools.
If you need to exec festival scripts using a version of
festival built with shared libaries, you must either execute
them from a featureful shell (e.g. *bash*), 
install the shared libraries in a standard place or explicitly
set *LD_LIBRARY_PATH*.  Solaris's standard
shell doesn't support script excutaiton of shells within shells.

Simple choices for common set ups are given near the top of this
file.  But for some sub-systems you will also need to change pathnames for
external library support.


At present read *config/ReadMe* for details of changing 
basic configuration.


On systems (and versions of systems) we have not yet encountered
you may need to create `config/systems/<PROCESSOR>_<OS><OSVERSION>.mak`
files.  Often you need only copy an existing one (other version or
similar system) to get it to work.  Typically the only real differences
are when there are major differences in the default C++ compiler
(e.g. RedHat 5.0 to RedHat 5.1).  If you do need to add a new
systems configuration file please let as know so we can include it
in the standard distribution.

### Compilation

Once you have configured *config/config* you
can compile the system.

    gnumake

Note this must be **gnumake**, which may be called *make* on
your system, or *gmake* or *gnumake*.  This will compile all
library functions and all the executables.  If you wish to only compile
the library itself then use

    gnumake make_library

Note that if you compile with *-g* (uncommenting *DEBUG = 1*
is *config/config* the library and the corresponding binaries will
be large.  Particulary the executables, you will need in order of 150
megabytes to compile the system, if your C++ libraries are not compiled
as shared libraries.  If you compile without *-g* the whole library
directory is about 12 megabytes on Linux (which has shared libraries for
*libstdc++* or about 26 megabytes of Sparc Solaris (which does not
have a shared library *libstdc++* by default).  This is almost
entirely due to the size of the executables.  C++ does not make small
binaries.


In general we have made the system compile with no warnings.  However
for some compilers this has proved to be near impossible.  SunOS include
files have a number of system declarations missing, so many system functions
(e.g. *fprintf*) will appear to be undeclared.  Sun's CC compiler
also likes to complain about missing source for some code even though
the code exists within our system and is deliberately in separate files
ro make it modular.


To test the system after compilation 

    gnumake test

## Installation {#estinstallinstalling}


All executables are linked to from *speech_tools/bin* and you
should add that to your *PATH* in order to use them.


Include files are *speech_tools/include/* and the three
generated libraries are *speech_tools/lib/libestools.a*,
*speech_tools/lib/libestbase.a* and
*speech_tools/lib/libestring.a*.  For most cases the three 
will be required.


If space is a preminium, compiled with the shared option (binaries
will be then be substantially smaller) and you can delete all 
*.o* files.

Some aspects of the system have further dependencies which depend
of the option selected at compile time.  Specifically the readline
libraries and Netaudio libraries.


## Building on Windows 95/98/NT {#estinstallbuildwind}

There are two ways to build the system under Windows. The
Cygwin system provides a unix-like
environment in which you can perform a compilation as described in the
previous sections. Cygwin is probably the
best choice if you can use that route.


Visual C provides an environment much further from the Unix systems
our code is developed on, this places some limits on the Visual C port,
especially in areas relating to networking. The remainder of this
section describes how to compile with Visual C. 

### Creating *VCMakefile*

We compile using the *nmake* program which comes with
Visual C. This is a fairly standard *make*
implementation, but is not as flexible as the *GNU make* 
we use for our normal builds. So, in order to
compile with *nmake* new *Makefile* files
are needed. These can be created from the Unix makefiles using 

    gnumake VCMakefile
    Creating VCMakefile for .
    Creating VCMakefile for include
    Creating VCMakefile for include/unix
    [...]

Obviously you will need either a unix machine or the
Cygwin system to do this. Sharing the
compilation directory between unix and Windows machines, for instance
using *samba*.


### Configuration

As for unix compilations, the Visual C compilation process is
controlled by a configuration file. In this case it is called
*vc_config_make_rules*. A version is included in
the distribution, as *vc_config_make_rules-dist*,
copy this into place as follows:

    cd config
    cp vc_config_make_rules-dist vc_config_make_rules
    chmod +w vc_config_make_rules

You probably don't need to change this default configuration.

### Building

To build the system:

    nmake /nologo /fVCMakefile

This should build the libraries and executables, and also the test
programs in *testsuite*. However there is currently
no way to automatically test the system, Indeed some of the test
programs will fail under Visual C due to differences in file nameing
conventions. 


