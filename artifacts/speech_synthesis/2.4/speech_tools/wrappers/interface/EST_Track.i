/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                 (University of Edinburgh, UK) and                     */
/*                           Korin Richmond                              */
/*                         Copyright (c) 2003                            */
/*                         All Rights Reserved.                          */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*                                                                       */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*                   Author :  Korin Richmond                            */
/*                     Date :  02 June 2003                              */
/* -------------------------------------------------------------------   */
/*       EST_Track script language interface file                        */
/*                                                                       */
/*************************************************************************/

%module EST_Track

%{
#include "EST_Track.h"
#include "EST_track_aux.h"
%}

%include "typemaps.i"
%include "EST_rw_status.i"

%import "EST_FVector.i"
%include "EST_typemaps.i"

class EST_Track {
  
protected:
  EST_FMatrix p_values;		
  EST_FVector p_times;	        
  EST_CVector p_is_val;		
  
  EST_ValMatrix p_aux;		
  EST_StrVector p_aux_names;		

  float p_t_offset;			
  
  EST_TrackMap::P p_map;
  EST_StrVector p_channel_names;	
  
  bool p_equal_space;			
  bool p_single_break;		 
  
  void default_vals();
  void default_channel_names();
  void clear_arrays();
  void pad_breaks();
    
  int interp_value(float x, float f);
  float interp_amp(float x, int c, float f);
  float estimate_shift(float x);
  void copy(const EST_Track& a);

public:
  // these are static in the EST_Track class, but there's some
  // problem with segmentation fault...
  const float default_frame_shift;
  const int default_sample_rate;
  
  EST_Track();
  EST_Track(const EST_Track &a);
  EST_Track(int num_frames, int num_channels);
  ~EST_Track();
  
  void resize(int num_frames, int num_channels, bool preserve = 1);
  
  void set_num_channels(int n, bool preserve = 1);
  void set_num_frames(int n, bool preserve = 1);
  
  void set_channel_name(const EST_String &name, int channel);
  
  // set the name of the auxiliary channel.
  void set_aux_channel_name(const EST_String &name, int channel);
  
  // copy everything but data
  void copy_setup(const EST_Track& a); 
  
  // name of track
  EST_String name() const;
  void set_name(const EST_String &n);

  // make "fv" a window to frame "n" in the track. 
  void frame(EST_FVector &fv, int n, int startf=0, int nf=EST_ALL);
  
  // make "fv" a window to channel "n" in the track. 
  void channel(EST_FVector &cv, int n, int startf=0, int nf=EST_ALL);

  // make "fv" a window to the named channel in the track. 
  void channel(EST_FVector &cv, const char * name, int startf=0, int nf=EST_ALL);

  void sub_track(EST_Track &st,
	         int start_frame=0, 
		 int nframes=EST_ALL,
		 int start_chan=0, 
		 int nchans=EST_ALL);
  
  
  void copy_sub_track(EST_Track &st,
		      int start_frame=0, 
		      int nframes=EST_ALL,
		      int start_chan=0, 
		      int nchans=EST_ALL) const;

  void copy_sub_track_out( EST_Track &st, 
			   const EST_FVector& frame_times ) const;
  
  void copy_sub_track_out( EST_Track &st, 
			   const EST_IVector& frame_indices ) const;

  // copy channel "n" into pre-allocated buffer "buf"
  //  void copy_channel_out(int n, float *buf, int offset=0, int nf=EST_ALL) const;

  // copy channel {\tt n} into EST_FVector
  void copy_channel_out(int n, EST_FVector &f, int offset=0, int nf=EST_ALL) const;
 
  // copy frame "n" into pre-allocated buffer "buf"
  //  void copy_frame_out(int n, float *buf, int offset=0, int nc=EST_ALL) const; 

  // copy frame "n" into EST_FVector "f"
  void copy_frame_out(int n, EST_FVector &f, int offset=0, int nc=EST_ALL) const;

  // copy "buf" into pre-allocated channel "n" of track
  //  void copy_channel_in(int n, const float *buf, int offset=0, int nf=EST_ALL);

  // copy f into pre-allocated channel n of track */
  void copy_channel_in(int n, const EST_FVector &f, int offset=0, int num=EST_ALL);

  // copy channel "buf" into pre-allocated channel "n" of track 
  void copy_channel_in(int c, 
		       const EST_Track &from, int from_c, int from_offset=0,
		       int offset=0, int num=EST_ALL);

  // copy "buf" into frame "n" of track 
  //  void copy_frame_in(int n, const float *buf, int offset=0, int nc=EST_ALL);

  // copy "f" into frame "n" of track
  void copy_frame_in(int n, const EST_FVector &f, int offset=0, int nf=EST_ALL);

  // copy from "from" frame into frame "n" of track 
  void copy_frame_in(int i, 
		     const EST_Track &from, int from_f, int from_offset=0, 
		     int offset=0, int num=EST_ALL);

  // Return the position of channel "name" if it exists,otherwise return -1.
  int channel_position(const char *name, int offset=0) const;
  
  bool has_channel(const char *name) const;

  // return amplitude of frame i, channel c.
  float a(int i, int c=0) const;

  // return amplitude at frame nearest t, channel c.
  float a(float t, int c=0) const;
  
  // return time position of frame i
  float  t(int i=0) const;
  
  // return time of frame i in milli-seconds.
  float ms_t(int i) const;

  // set frame times to regular intervals of time "t".
  //void fill_time(float t, int start =1);

  // set frame times to regular intervals of time "t",
  // begining at time "startt"
  void fill_time(float t, float startt=0.0);

  // fill times with times of other track
  void fill_time(EST_Track &t);

  // fill all amplitudes with value "v"
  void fill(float v);

  // resample track at this frame shift, specified in seconds. 
  void sample(float shift);

  // return an estimation of the frame spacing in seconds. 
  float shift() const;
  // return time of first value in track
  float start() const;
  // return time of last value in track
  float end() const;

//  EST_read_status load(const EST_String name, float ishift = 0.0, float startt = 0.0);
 // EST_write_status save(const EST_String name, const EST_String EST_filetype = "");

  EST_read_status load(const char *name, float ishift = 0.0, float startt = 0.0);
  EST_write_status save(const char *name, const char* EST_filetype = "");

  // set frame i to be a break
  void set_break(int i);
  
  // set frame i to be a value
  void set_value(int i);

  // return true if frame i is a value
  int val(int i) const;

  // return true if frame i is a break
  int track_break(int i) const { return (p_is_val(i)); }

  /** starting at frame i, return the frame index of the first
  	value frame before i. If frame i is a value, return i */
  int prev_non_break(int i) const;

  /** starting at frame i, return the frame index of the first
  	value frame after i. If frame i is a value, return i */
  int next_non_break(int i) const;

  int empty() const;
  
  // return the frame index nearest time t
  int index(float t) const;		

  // return the frame index before time t
  int index_below(float x) const;

  // return number of frames in track
  int num_frames() const;

  // return number of frames in track
  int length() const;
  
  // return number of channels in track
  int num_channels() const;
  
  // return number of auxiliary channels in track
  int num_aux_channels() const;

  // return true if track has fixed frame spacing 
  bool equal_space() const;

  // return true if track has only single breaks between value sections
  bool single_break() const;

  void set_equal_space(bool t);
  void set_single_break(bool t);

  //  EST_Track& operator = (const EST_Track& a);
  // add track at end
  EST_Track& operator+=(const EST_Track &a); 
  // add track in parallel
  EST_Track& operator|=(const EST_Track &a); 
 
  EST_read_status load_channel_names(const EST_String name);
  EST_write_status save_channel_names(const EST_String name);

  const EST_String channel_name(int channel, int strings_override=1) const;

  const EST_String aux_channel_name(int channel) const; 
};

float mean( const EST_Track &tr, int channel );
void mean( const EST_Track &tr, EST_FVector &means );

%apply float &OUTPUT { float &m, float &sd };
void meansd( EST_Track &tr, float &m, float &sd, int channel );
%clear float &m, float &sd;

void meansd( EST_Track &tr, EST_FVector &m, EST_FVector &sd );

void normalise( EST_Track &tr );

void normalise( EST_Track &tr, float mean, float sd, int channel, 
	        float upper, float lower );

void normalise( EST_Track &tr, EST_FVector &mean, EST_FVector &sd,
	        float upper, float lower);
