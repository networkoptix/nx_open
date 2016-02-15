/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1996                            */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
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
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*             Author :  Paul Taylor and Alan Black                      */
/*             Date   :  May 1996                                        */
/*-----------------------------------------------------------------------*/
/*            EST_Wave Class source file                                 */
/*                                                                       */
/*=======================================================================*/

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "EST_cutils.h"
#include "EST_Wave.h"
#include "EST_wave_utils.h"
#include "EST_wave_aux.h"
#include "EST_TNamedEnum.h"
#include "EST_WaveFile.h"

#include "EST_Track.h"

#include "waveP.h"

const EST_String DEF_FILE_TYPE = "riff";
const EST_String DEF_SAMPLE_TYPE = "short";

const int EST_Wave::default_sample_rate=16000;

EST_Wave::EST_Wave()
{
    default_vals();
}

EST_Wave::EST_Wave(const EST_Wave &w)
{
    default_vals();
    copy(w);
}

EST_Wave::EST_Wave(int n, int c, int sr)
{
  default_vals(n,c);
  set_sample_rate(sr);
}

EST_Wave::EST_Wave(int samps, int chans,
		   short *memory, int offset, int sample_rate, 
		   int free_when_destroyed)
{
  default_vals();
  p_values.set_memory(memory, offset, samps, chans, free_when_destroyed);
  set_sample_rate(sample_rate);
}

void EST_Wave::default_vals(int n, int c) 
{
  // real defaults
  p_values.resize(n,c);
  p_sample_rate = default_sample_rate;

  init_features();
}

void EST_Wave::free_wave()
{
  if (!p_values.p_sub_matrix)
    p_values.resize(0,0);
  clear_features();
}

EST_Wave::~EST_Wave()
{
  free_wave();
}

void EST_Wave::copy_setup(const EST_Wave &w)
{
    p_sample_rate = w.p_sample_rate;
    copy_features(w);
}

void EST_Wave::copy_data(const EST_Wave &w)
{
    p_values.copy(w.p_values);
}

void EST_Wave::copy(const EST_Wave &w)
{
    copy_setup(w);

    copy_data(w);
}

short &EST_Wave::a(int i, int channel)
{
  if (i<0 || i>= num_samples())
    {
      cerr << "Attempt to access sample " << i << " of a " << num_samples() << " sample wave.\n";
      if (num_samples()>0)
	return *(p_values.error_return);
    }

  if (channel<0 || channel>= num_channels())
    {
      cerr << "Attempt to access channel " << channel << " of a " << num_channels() << " channel wave.\n";
      if (num_samples()>0)
	return *(p_values.error_return);
    }

  return p_values.a_no_check(i,channel);
}

short EST_Wave::a(int i, int channel) const
{
  return ((EST_Wave *)this)->a(i,channel);
}

short &EST_Wave::a_safe(int i, int channel)
{
    static short out_of_bound_value = 0;

    if ((i < 0) || (i >= num_samples()))
    {   // need to give them something but they might have changed it
	// so reinitialise it to 0 first
	out_of_bound_value = 0;
	return out_of_bound_value;
    }
    else
	return a_no_check(i,channel);
}

void EST_Wave::fill(short v, int channel)
{
    if (channel == EST_ALL)
    {
	if (v == 0)  // this is *much* more efficient and common
	    memset(values().memory(),0,num_samples()*num_channels()*2);
        else
	    p_values.fill(v);
    }
    else 
      for (int i = 0; i < num_samples(); ++i)
	p_values.a_no_check(i,channel) = v;
}

EST_read_status EST_Wave::load(const EST_String filename, 
			       int offset, int length,
			       int rate)
{
    EST_read_status stat = read_error;
    EST_TokenStream ts;

    if ((ts.open(filename)) == -1)
    {
	cerr << "Wave load: can't open file \"" << filename << "\"" << endl;
	return stat;
    }

    stat = load(ts,offset,length,rate);
    ts.close();
    return stat;
}

EST_read_status EST_Wave::load(EST_TokenStream &ts,
			       int offset, int length,
			       int rate)
{
    EST_read_status stat = read_error;
    int pos = ts.tell();
    
    for(int n=0; n< EST_WaveFile::map.n() ; n++)
    {
	EST_WaveFileType t = EST_WaveFile::map.token(n);
	
	if (t == wff_none)
	    continue;
	
	EST_WaveFile::Info *info = &(EST_WaveFile::map.info(t));
	
	if (! info->recognise)
	    continue;
	
	EST_WaveFile::Load_TokenStream * l_fun =info->load;
	
	if (l_fun == NULL)
	    continue;

	ts.seek(pos);
	stat = (*l_fun)(ts, *this, 
			rate, st_short, EST_NATIVE_BO, 1,
			offset, length);
	
	if (stat == read_ok)
	{
	    set_file_type(EST_WaveFile::map.value(t));
	    break;
	}
	else if (stat == read_error)
	    break;
    }
    
    return stat;
}

EST_read_status EST_Wave::load(const EST_String filename, 
			       const EST_String type, 
			       int offset, int length,
			       int rate)
{
    EST_read_status stat = read_error;
    EST_TokenStream ts;

    if (filename == "-")
	ts.open(stdin,FALSE);
    else if ((ts.open(filename)) == -1)
    {
	cerr << "Wave load: can't open file \"" << filename << "\"" << endl;
	return stat;
    }

    stat = load(ts,type,offset,length,rate);
    ts.close();
    return stat;
}

EST_read_status EST_Wave::load(EST_TokenStream &ts,
			       const EST_String type, 
			       int offset, int length,
			       int rate)
{
    EST_WaveFileType t = EST_WaveFile::map.token(type);

    if (t == wff_none)
    {
	cerr << "Unknown Wave file type " << type << endl;
	return read_error;
    }

    EST_WaveFile::Load_TokenStream * l_fun = EST_WaveFile::map.info(t).load;
    
    if (l_fun == NULL)
    {
	cerr << "Can't load waves to files type " << type << endl;
	return read_error;
    }
    
    set_file_type(EST_WaveFile::map.value(t));
    return (*l_fun)(ts, *this,
		    rate, st_short, EST_NATIVE_BO, 1,
		    offset, length);
   
}

EST_read_status EST_Wave::load_file(const EST_String filename, 
		     const EST_String type, int sample_rate,
		     const EST_String stype, int bov, int nc, int offset,
		     int length)
{
    EST_read_status stat = read_error;
    EST_TokenStream ts;

    if (filename == "-")
	ts.open(stdin,FALSE);
    else if ((ts.open(filename)) == -1)
    {
	cerr << "Wave load: can't open file \"" << filename << "\"" << endl;
	return stat;
    }

    stat = load_file(ts,type,sample_rate,stype,bov,nc,offset,length);
    ts.close();
    return stat;
}

EST_read_status EST_Wave::load_file(EST_TokenStream &ts,
		     const EST_String type, int sample_rate,
		     const EST_String stype, int bov, int nc, int offset,
		     int length)

{
  EST_WaveFileType t = EST_WaveFile::map.token(type);

  EST_sample_type_t values_type = EST_sample_type_map.token(stype);

  if (t == wff_none)
    {
      cerr << "Unknown Wave file type " << type << endl;
      return read_error;
    }

  EST_WaveFile::Load_TokenStream * l_fun = EST_WaveFile::map.info(t).load;
    
  if (l_fun == NULL)
    {
      cerr << "Can't load waves to files type " << type << endl;
      return read_error;
    }
    
  return (*l_fun)(ts, *this,
		  sample_rate, values_type, bov, nc,
		  offset, length);
    
}    

void EST_Wave::sub_wave(EST_Wave &sw, 
			int offset, int num,
			int start_c, int nchan)
{
  if (num == EST_ALL)
    num = num_samples()-offset;
  if (nchan == EST_ALL)
    nchan = num_channels()-start_c;

  p_values.sub_matrix(sw.p_values, offset, num, start_c, nchan);
  sw.set_sample_rate(sample_rate());
}

EST_write_status EST_Wave::save(const EST_String filename, 
				 const EST_String type)
{
    FILE *fp;

    if (filename == "-")
	fp = stdout;
    else if ((fp = fopen(filename,"wb")) == NULL)
    {
	cerr << "Wave save: can't open output file \"" <<
	    filename << "\"" << endl;
	return write_fail;
    }

    EST_write_status r = save(fp,type);
    if (fp != stdout)
	fclose(fp);
    return r;
}

EST_write_status EST_Wave::save(FILE *fp, const EST_String type)
{
    EST_String save_type = (type == "") ? DEF_FILE_TYPE : type;

    EST_WaveFileType t = EST_WaveFile::map.token(save_type);

    if (t == wff_none)
    {
	cerr << "Wave: unknown filetype in saving " << save_type << endl;
	return write_fail;
    }
    
    EST_WaveFile::Save_TokenStream * s_fun = EST_WaveFile::map.info(t).save;
    
    if (s_fun == NULL)
    {
	cerr << "Can't save waves to files type " << save_type << endl;
	return write_fail;
    }
    
    return (*s_fun)(fp, *this, st_short, EST_NATIVE_BO);
}

EST_write_status EST_Wave::save_file(const EST_String filename,
				     EST_String ftype,
				     EST_String stype, int obo)
{
    FILE *fp;

    if (filename == "-")
	fp = stdout;
    else if ((fp = fopen(filename,"wb")) == NULL)
    {
	cerr << "Wave save: can't open output file \"" <<
	    filename << "\"" << endl;
	return write_fail;
    }

    EST_write_status r = save_file(fp,ftype,stype,obo);
    if (fp != stdout)
	fclose(fp);
    return r;
}

EST_write_status EST_Wave::save_file(FILE *fp,
				     EST_String ftype,
				     EST_String stype, int obo)
{
    EST_WaveFileType t = EST_WaveFile::map.token(ftype);
    EST_sample_type_t sample_type = EST_sample_type_map.token(stype);

    if (t == wff_none)
      {
	cerr << "Unknown Wave file type " << ftype << endl;
	return write_fail;
      }

    EST_WaveFile::Save_TokenStream * s_fun = EST_WaveFile::map.info(t).save;
    
    if (s_fun == NULL)
    {
	cerr << "Can't save waves to files type " << ftype << endl;
	return write_fail;
    }
    
    return (*s_fun)(fp, *this, sample_type, obo);
    
}

void EST_Wave::resample(int new_freq)
{
    // Resample wave to new sample rate
    if (new_freq != p_sample_rate)
    {
	if (p_values.rateconv(p_sample_rate, new_freq) != 0)
	    cerr << "rateconv: failed to convert from " << p_sample_rate <<
		" to " << new_freq << "\n";
	else
	    set_sample_rate(new_freq);
    }
    
}

void EST_Wave::rescale(float gain, int normalize)
{
    int ns;
    float factor = gain;
    float nsf;
    
    if (normalize)
    {
	int max = 0;
	for (int i = 0; i < num_samples(); ++i)
	  for (int j = 0; j < num_channels(); ++j)
	    if (abs(a_no_check(i,j)) > max)
		max = abs(a_no_check(i,j));
	if (fabs(max/32766.0-gain) < 0.001)
	    return; /* already normalized */
	else
	    factor *= 32766.0/(float)max;
    }
    
    for (int i = 0; i < num_samples(); ++i)
      for (int j = 0; j < num_channels(); ++j)
	{
            if (factor == 1.0)
                ns = a_no_check(i,j);  // avoid float fluctuations
            else if (factor == -1.0)
                ns = -a_no_check(i,j);  // avoid float fluctuations
            else
            {
                nsf = (float)a_no_check(i,j) * factor;
                if (nsf < 0.0)
                    ns = (int)(nsf - 0.5);
                else
                    ns = (int)(nsf + 0.5);
            }
	  if (ns < -32766)
	    a_no_check(i,j)= -32766;
	  else if (ns > 32766)
	    a_no_check(i,j)= 32766;
	  else
	    a_no_check(i,j)= ns;
	}
}

void EST_Wave::rescale( const EST_Track &fc )
{
  int ns, start_sample, end_sample;
  float target1, target2, increment, factor, nsf;
  
  int fc_length = fc.length();
  int _num_channels = num_channels();

  cerr << ((int)(fc.t(fc_length-1) * p_sample_rate)) << endl;

  if( ((int)(fc.t(fc_length-1) * p_sample_rate)) > num_samples() )
    EST_error( "Factor contour track exceeds waveform length (%d samples)",
		 (fc.t(fc_length-1) * p_sample_rate) - num_samples() );

  start_sample = static_cast<unsigned int>( fc.t( 0 )*p_sample_rate );
  target1 = fc.a(0,0); // could use separate channels for each waveform channel

  for ( int k = 1; k<fc_length; ++k ){
    end_sample   = static_cast<unsigned int>( fc.t( k )*p_sample_rate );
    target2 = fc.a(k);
    
    increment = (target2-target1)/(end_sample-start_sample+1);
  
    factor = target1;
    for( int i=start_sample; i<end_sample; ++i, factor+=increment )
      for( int j=0; j<_num_channels; ++j ){
            if (factor == 1.0)
                ns = a_no_check(i,j);  // avoid float fluctuations
            else if (factor == -1.0)
                ns = -a_no_check(i,j);  // avoid float fluctuations
            else
            {
                nsf = (float)a_no_check(i,j) * factor;
                if (nsf < 0.0)
                    ns = (int)(nsf - 0.5);
                else
                    ns = (int)(nsf + 0.5);
            }
          if (ns < -32766)
              a_no_check(i,j)= -32766;
          else if (ns > 32766)
              a_no_check(i,j)= 32766;
          else
              a_no_check(i,j)= ns;
      }
    start_sample = end_sample;
    target1 = target2;
  }
}



EST_Wave &EST_Wave::operator =(const EST_Wave &w)
{
    copy(w);
    return *this;
}

EST_Wave &EST_Wave::operator +=(const EST_Wave &w)
{
    EST_Wave w2;
    const EST_Wave *toadd = &w;

    if (w.num_channels() != num_channels())
    {
	cerr << "Cannot concatenate waveforms with differing numbers of channels\n";
	return *this;
    }

    if (p_sample_rate != w.sample_rate())
    {
	w2 = w;
	w2.resample(p_sample_rate);
	toadd= &w2;
    }

    p_values.add_rows(toadd->p_values);

    return *this;
}

// add wave p_values to existing wave in parallel to create multi-channel wave.
EST_Wave &EST_Wave::operator |=(const EST_Wave &wi)
{
    int i, k;
    EST_Wave w = wi; // to allow resampling of const

    w.resample(p_sample_rate);  // far too difficult otherwise

    int o_channels = num_channels();
    int r_channels = num_channels()+w.num_channels();
    int r_samples = Gof(num_samples(), w.num_samples());

    resize(r_samples, r_channels);

    for (k = 0; k < w.num_channels(); ++k)
	for (i=0; i < w.num_samples(); i++)
	    a(i,k+o_channels) += w.a(i, k);

    return *this;
}

ostream& operator << (ostream& p_values, const EST_Wave &sig)
{
    for (int i = 0; i < sig.num_samples(); ++i)
	p_values << sig(i) << "\n";
    
    return p_values;
}

int operator != (EST_Wave a, EST_Wave b)
   { (void)a; (void)b; return 1; }
int operator == (EST_Wave a, EST_Wave b)
  {  (void)a; (void)b; return 0; }
