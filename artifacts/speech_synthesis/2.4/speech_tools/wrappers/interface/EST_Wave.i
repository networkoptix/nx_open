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
/*             Author :  Korin Richmond                                  */
/*               Date :  02 June 2003                                    */
/* -------------------------------------------------------------------   */
/*   EST_Wave Class interface file                                       */
/*                                                                       */
/*************************************************************************/

%module EST_Wave

%{
#include "EST_Wave.h"
#include "EST_wave_aux.h"
#include "EST_audio.h"
#include "EST_Option.h"
%}

%include "EST_rw_status.i"
%include "EST_typemaps.i"

class EST_Wave
{
protected:
  EST_SMatrix p_values;

  int p_sample_rate;

  void default_vals(int n=0, int c=1);
  void free_wave();
  void copy_data(const EST_Wave &w);
  void copy_setup(const EST_Wave &w);

public:

  // these are static in the EST_Wave class, but there's some
  // problem with segmentation fault...
  const int default_sample_rate;
  //const int default_num_channels;

  EST_Wave();
  EST_Wave(const EST_Wave &a);
  EST_Wave(int n, int c, int sr);
  ~EST_Wave();
    
  short a(int i, int channel = 0) const;
  short &a_safe(int i, int channel = 0);

  // set value
  short set_a(int i, int channel = 0, short val = 0);

  // return the time position in seconds of the ith sample
  float t(int i) const;

  int num_samples() const;
  int num_channels() const;
  int sample_rate() const;
  void set_sample_rate(const int n);
  int length() const;

  // return the time position of the last sample.
  float end();

  // Can we look N samples to the left?
  bool have_left_context(unsigned int n) const;

  EST_String sample_type() const;
  void set_sample_type(const EST_String t);

  EST_String file_type() const;
  void set_file_type(const EST_String t);

  EST_String name() const;
  void set_name(const EST_String n);

  void resize(int num_samples, int num_channels = EST_ALL, int set=1);
  void resample(int rate);

  // multiply all samples by factor "gain".
  void rescale( float gain,int normalize=0 );
  void rescale( const EST_Track &factor_contour );

  // clear waveform and set size to 0.
  void clear();

  void copy(const EST_Wave &from);
  void fill(short v=0, int channel=EST_ALL);

  void empty(int channel=EST_ALL);

  EST_read_status load(const EST_String filename, 
		       int offset=0, 
		       int length = 0,
		       int rate = default_sample_rate);

  EST_read_status load_file(const EST_String filename, 
			    const EST_String filetype, int sample_rate, 
			    const EST_String sample_type, int bo, int nc,
			    int offset = 0, int length = 0);

  EST_write_status save(const  EST_String filename, 
			const EST_String EST_filetype = "");

  EST_write_status save_file(const EST_String filename, 
			     EST_String filetype,
			     EST_String sample_type, int bo);

  void integrity() const;

  %extend {
    void info()
      {
	wave_info( *self );
      }
  }  
  
  %extend {
    void play()
      {
	EST_Option empty;
	play_wave( *self, empty );
      }
  }
};


int wave_extract_channel(EST_Wave &single, const EST_Wave &multi, int channel);

void wave_combine_channels(EST_Wave &combined, const EST_Wave &multi);

int wave_subwave(EST_Wave &subsig,EST_Wave &sig,int offset,int length);

int wave_divide(EST_WaveList &wl, EST_Wave &sig, EST_Relation &keylab,
                const EST_String &ext);

int wave_extract(EST_Wave &part, EST_Wave &sig, EST_Relation &keylab, 
                 const EST_String &file);

void add_waves(EST_Wave &s, const EST_Wave &m);

EST_Wave difference(EST_Wave &a, EST_Wave &b);
float rms_error(EST_Wave &a, EST_Wave &b, int channel);
float abs_error(EST_Wave &a, EST_Wave &b, int channel);
float correlation(EST_Wave &a, EST_Wave &b, int channel);

EST_FVector rms_error(EST_Wave &a, EST_Wave &b);
EST_FVector abs_error(EST_Wave &a, EST_Wave &b);
EST_FVector correlation(EST_Wave &a, EST_Wave &b);

EST_Wave error(EST_Wave &ref, EST_Wave &test, int relax);

void absolute(EST_Wave &a);

void wave_info(EST_Wave &w);
void invert(EST_Wave &sig);

void differentiate(EST_Wave &sig);
void reverse(EST_Wave &sig);
