Introduction to the Edinburgh Speech Tools         {#estintro}
===========================================

[TOC]

The Edinburgh Speech Tools Library is a library of general speech software, written at the Centre for Speech Technology Research at the University of Edinburgh.

The Edinburgh Speech Tools Library is written is C++ and provides a range of
tools for common tasks found in speech processing.  The library provides a
set of stand alone executable programs and a set of library calls
which can be linked into user programs.

Updates and news about the speech tools include releases and bug fixes
may be found at [Speech Tools website](http://www.cstr.ed.ac.uk/projects/speech_tools.html)

Most speech researchers spend a considerable amount of time writing,
developing and debugging code. In fact, many researchers spend most of
their time doing this. The sad fact is that most of this time is spent
on unnecessary tasks - time which could be better spent doing "real"
research.  The library is intended to provide software that
programmers use day-to-day, and provide this in an easy to use fashion.

# The Library  {#thelibrary}

The Edinburgh Speech Tools Library has two main parts: a software
library and a set of programs which use the library.

A library is a single central place where useful software is kept.  A
UNIX library is a single file (in this case called `libestools.a`)
which can be linked to an individual program. When writing a program,
you can call any of the functions in the library, and they will
automatically be linked into your program when you compile. The key
point is that you never need look at the library itself or copy the
code in it. That way you can write small programs, concentrate on the
algorithms and not have to worry about any infrastructure issues.

The speech tools also provide a number of utility programs for things
like playback, sampling rate conversion, file format conversion,
etc. Usually these programs are just wrap-around executables based on
standard speech tools library functions.

## What does the library contain?

### Speech class
Includes tracks for storing sets of time aligned coefficients, and waves for
digitally sampled speech waveforms.

### Linguistic class
A comprenhensive system for storing different kinds of linguistic information
is given. This is based on the Hetrogeneous Relation Graph
formalism ***CITE***. Feature structures, tress, lists, graphs
ect can all be represented with the linguistic classes.

### Audio playback
Easy to use routines to record and play audio data without any fuss.

### Signal processing
Commonly used signal processing algorithms such as including pitch tracking, cepstra and LPC, filtering, fourier analysis etc.


### Statistical functions

### Grammars

### Intonation
Software support for the Tilt intonation model

### Speech Recognition

### Utility Functions and Classes
Useful classes such as lists, vectors, matrices, strings and functions for
reading files, parsing command lines etc.

## Using the speechtools
Once installed, speech tools can be used either by running the
exectuable programs or by building your own C++ programs and linking
to the library.

Documentation on executable programs is found in [Executable Programs](@ref estexec).

Instructions on how to build your own programs which use the library
are found in [Building speech tools](@ref estinstallbuild).

