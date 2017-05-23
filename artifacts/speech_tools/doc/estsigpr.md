Signal Processing {#estsigpr}
========================

The EST signal processing library provides a set of standard
signal processing tools designed specifically for speech
analysis. The library includes:

  - Windowing (creating frames from a continuous waveform)
  - Linear prediction and associated operations
  - Cepstral analysis, both via lpc and DFT.
  - Filterbank analysis
  - Frequency warping including mel-scaling
  - Pitch tracking
  - Energy and Power analysis
  - Spectrogram Generation
  - Fourier Transforms
  - Pitchmarking (of laryngograph signals)

# Overview {#estsigproverview}

## Design Issues {#estsigprdesign}

The signal processing library is designed specifically for speech
applications and hence all functions are written with that end
goal in mind.  The design of the library has centered around
building a set of commonly used easy to configure analysis
routines.

  - **Speed**: We have tried to make the functions as fast as
    possible. Signal processing can often be time critical, and
    so it will always be the case that if the code for a
    particular signal processing algroithm is written in a
    single function loop it will run faster than by using
    libraries.
    
    However, the signal processing routines in the EST library
    are in general very fast, and the fact that they use
    classes such as EST_Track and EST_FVector does not make
    them slower than they would be if `float *` etc was used.

  - **types**: The library makes heavy use of a small number of
    classes, specifically EST_Wave, EST_Track and EST_FVector. These 
    classes are basically arrays and matrices, but take care of 
    issues such as memory managment, error handling and file i/o. Using 
    these classes in the library helps facilitate clean and simple
    algorithm writing and use. It is strongly recommended that
    you gain familiarity with these classes before using this
    part of the library.
    
    At present, the issue of complex numbers in signal
    processing is somewhat fudged, in that a vector of complex
    numbers is represented by a vector of real parts and a
    vector of imaginary parts, rather than as a single vector
    of complex numbers. 

## Common Processing model {#estsigprcommonprocessing}

In speech, a large number of algorithms follow the same basic
model, in which a waveform is analysed by an algorithm and a
Track, containing a series of time aligned vectors is
produced. Regardless of the type of signal processing, the
basic model is as follows:

  1. Start with a waveform and a series of analysis positions, which 
     can be a fixed distance apart of specified by some other means.
  2. For each analysis position, define a small portion of the
     waveform around that position, Multiply this by a
     windowing function to produce a vector of speech samples.
  3. Pass this to a frame based signal processing 
     routine which outputs values in another vector.
  4. Add this vector to a position in an EST_Track
     which correponds to the analysis time position.

Given this model, the signal processing library breaks down into a
number of different types of function:

 - **Utterance based functions**:  Functions which operate on an entire waveform or
	 track. These break down into:
    - **Analysis Functions**: which take a waveform and produce a track
    - **Synthesis Functions**: which take a track and produce a waveform
    - **Filter Functions**: which take a waveform and produce a waveform
    - **Conversion Functions**: which take a track and produce a track
 - **Frames based functions**:  Functions which operate on a single frame of speech or
   vector coefficients.
 - **Windowing functions**: which create a windowed frame of speech from a portion
   of a waveform.

Nearly all functions in the signal processing library belong to
one of the above listed types. Quite often functions are
presented on both the utterance and frame level. For example,
there is a function called \ref sig2lpc which
takes a single frame of windowed speech and produces a set of
linear prediction coefficients. There is also a function called
\ref sig2coef which performs linear prediction
on a whole waveforn, returning the answer in a
Track. \ref sig2coef uses the common processing
model, and calls \ref sig2lpc as the algorithm
in the loop.

Partly for historical reasons some functions,
e.g. \ref pda are only available in the
utterance based form.

When writing signal processing code for this library, it is
often the case that all that needs to be written is the frame
based algorithm, as other algorithms can do the frame shifting
and windowing operations.

   
## Track Allocation, Frames, Channels and sub-tracks {#estsigprtrackalloc}

The signal processing library makes extensive use of the
advanced features of the track class, specifically the ability
to access single frames and channels.  

Given a standard multi-channel track, it is possible to make 
a FVector point to any single frame or channel - this is done
by an internal pointer mechanism in EST_FVector. Furthermore,
a track can be made to point to a selected number of channels
or frames in a main track.

For example, imagine we have a function that calculates the
covariance matrix for a multi-dimensional track of data.  But
the data we actually have contains energy, cepstra and delta
cepstra.  It is non-sensical to calculate convariance on
all of this, we just want the cepstra. To do this we use the
sub-track facility to set a temporary track to just the
cepstral coefficients and pass this into the covariance
function. The temporary track has smart pointers into the
original track and hence no data is copied.

Without this facility, either you would have to do a copy
(expensive) or else tell the covariance function which part of
the track to use (hacky).

Extensive documentation describing this process is found in \ref sigpr-example-frames,
\ref tr_example_access_multiple_frames and \ref tr_example_access_single_frames.

# Functions {#estsigprfunctions}

## Functions for Generating Frames {#est-sigpr-generating-frames}

The following set of functions perform either a signal
processing operation on a single frame of speech to produce a set of
coefficients, or a transformation on an existing set of coefficients
to produce a new set. In most cases, the first argument to the
function is the input, and the second is the output. It is assumed
that any input speech frame has already been windowed with an
appropriate windowing function (eg. Hamming) - see 
\ref "Windowing mechanisms" on how to produce such a frame. See also
\ref sigpr-track-func.

It is also assumed that the output vector is of the correct size. No
resizing is done in these functions as the incoming vectors may be
subvectors of whole tracks etc. In many cases (eg. lpc analysis), an
**order** parameter is required. This is usually derived from the size
of the input or output vectors, and hence is not passed explicitly.

  - \ref LinearPredictionfunctions
  - \ref Energyandpowerframefunctions
  - \ref FastFourierTransformfunctions
  - \ref Framebasedfilterbankandcepstralanalysis

## Functions for Generating Tracks {#sigpr-track-func}

Functions which operate on a whole waveform and generate coefficients
for a track.

  - \ref Functionsforusewithframebasedprocessing
  - \ref DeltaandAccelerationcoefficients
  - \ref PitchF0DetectionAlgorithmfunctions
  - \ref PitchmarkingFunctions
  - \ref Spectrogramgeneration

These functions are a nice set of stuff

## Functions for Windowing Frames of Waveforms   {#est_sigpr_windowing}

  - \ref EST_Window


## Filter functions {#sigpr-filter}

A filter modifies a waveform by changing its frequency
characteristics. The following types of filter are currently
supported:

  - **FIR filters**: FIR filters are general purpose finite impulse
    response filters which are useful for band-pass, low-pass and 
    high-pass filtering.
  - **Linear Prediction filters**:  are used to produce LP residuals
    from waveforms and viceversa.
  - **Pre Emphasis filters**: are simple filters for changing the 
    spectral tilt of a signal.
  - **Non linear filters**: Miscellaneous filters

  - \subpage FIRfilters
  - \subpage LinearPredictionfilters
  - \subpage PrePostEmphasisfilters
  - \subpage Miscellaneousfilters

## Filter design {#sigpr-filter-design}

  - \subpage FilterDesign

# Example
\subpage sigpr-example

# Programs {#sigpr-programs}

The following are exectutable programs which are used for signal
processing:

  - @ref sigfv_manual is used to provide produce a variety of feature vectors given a
    waveform.
  - @ref spectgen_manual is used to produce spectrograms from utterances.
  - @ref sigfilter_manual performs filtering operations on waveforms.
  - @ref pda_manual performs pitch detection on waveforms. While sig2fv can perform pitch
    detection also, pda offers more control over the operation.
  - @ref pitchmark_manual produces a set of pitchmarks, 
    specifying the instant of glottal close from laryngograph waveforms.

The following programs are also useful in signal processing:

  - @ref ch_wave_manual performs basic operations on waveforms, such as
    adding headers, resampling, rescaling, multi to single channel
    conversion etc.
  - @ref ch_track_manual performs basic operates on coefficient tracks,
    such as adding headers, resampling, rescaling, multi to single
    channel conversion etc.



