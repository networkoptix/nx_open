Speech Classes       {#estspeechclass}
================
[TOC]

# Overview {#overview}


EST offers two classes for handling and storing speech
information of all types: Waveforms and Tracks.  Both are
basically matrices with one dimension representing time, the
other representing a particular channel and the value at that
position representing an amplitude. There are signficant
differences between them, however, that makes the use of two
separate classes preferable to one.

## Waveforms {#waveforms}

Waveforms store digitalled sampled acoustic waveforms. They are
composed of a matrix of shorts, where rows represent individual
samples and columns represent channels. Waves can have arbitrarily
many channels, though 1 (mono) and 2 (stereo) are the most common.
Waves are stored as shorts as this is the most common file format,
which ensures fast compatibility with most file formats and hardware.
As each sample is representing by a 16-bit short, the dynamic range of
a wave is 96dB.

## The Track Class {#trackclass}

The track class is used to represent the outcome of a signal
processing operation on a section of speech. It can be thought of as
representing a series of *frames*, where each frame
represents signal processing information at a specified time point.


### The Amplitude Matrix

Each frame is a set of ordered coefficients, which represent the
output of a signal processing operation on a single section of
speech. For example, a frame may represent a spectrum, a cepstrum, or
a set of linear predication coefficients. An alternative view is to
visualise the track as a set of channels, where each channel
represents a how a particular type of information varies with
time. For instance, a channel might represent how the energy between
500Hz and 600Hz varies over the course of an utterance.

Frames and Channels are stored as a matrix of floats, where each point
in the matrix represents the amplitude of a given frame and a given
channel.

### The Time Array

In addition to the amplitude matrix, tracks also contain a
*time* array, which has the same number of elements
as frames in the amplitude matrix. The time array is aligned
one-to-one with the frames. Each position in the time array represents
the time of its frame. In many forms of signal processing, frames are
at fixed intervals (often 10ms), and in such cases it would be
possible to store this as a single global value. However, the track
class is extremely general in terms of time positions and allows
frames to be spaced irregularly, which is particularly useful when
dealing with pitch-synchronous processing.

### The Break/Value Array

The track class also contains a *break/value* array,
each element of which also as a one-to-one correspondence with a
frame. In many representations some frames have undefined values, and
the break/value array is used to represent this. For example, F0
contours and formants do not have values during unvoiced sections of
speech, and hence frames representing unvoiced sections may be tagged
as breaks in the break/value array. By default, it is assumed that all
amplitudes are defined and hence no breaks are set at contour
initialisation or resizing.

In time, this will be replaced by the more general Auxiliary Matrix.

### Channel names

trackmaps etc.

### The Auxiliary Matrix

It is inappropriate to store certain information in the amplitude array.

### Sub-tracks, channel and frame extaction

The track class provides an easy mechanism for dealing with a single
portion of the track at a time. If we have say a track with 10
channels and 500 frames, it is possible to assign a vector to any
single frame or channel, or to assign a sub-track to any contiguous
set of frames or channels. Any values that are changed in the frame or
channel vectors or sub-track, will affect the underlying tack. It is
of course possible to copy values in and out if values need to be
changed without changing the underlying track.


# Programs {#estspeechclassprograms}

The following programs are available:

  - @ref ch_wave_manual : performs basic operates on
    waveforms, such as adding headers, resampling, rescaling, multi to
    single channel conversion etc.
  - @ref ch_track_manual : performs basic operates on
    coefficient tracks, such as adding headers, resampling, rescaling,
    multi to single channel conversion etc.

# Classes {#estspeechclassclasses}

 - EST_Wave
 - EST_Track

# Functions {#estspecchclassfunctions}

## Auxiliary Track Functions {#estspeechclassfuncaux}

 - @ref EST_Track_aux_functions

# Examples

 - @subpage EST_Track-example


