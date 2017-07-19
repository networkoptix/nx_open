 /*************************************************************************/
 /*                                                                       */
 /*                Centre for Speech Technology Research                  */
 /*                     University of Edinburgh, UK                       */
 /*                      Copyright (c) 1995,1996                          */
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
 /*                                                                       */
 /*             Author : Richard Caley <rjc@cstr.ed.ac.uk>                */
 /* -------------------------------------------------------------------   */
 /* Wave file input and output.                                           */
 /*                                                                       */
 /*************************************************************************/
#include <cstdlib>
#include "EST_Wave.h"
#include "EST_WaveFile.h"
#include "waveP.h"
#include "EST_cutils.h"
#include "EST_Option.h"
#include "EST_io_aux.h"
#include "stdio.h"
#include "math.h"

void extract(EST_Wave &sig, EST_Option &al);

typedef 
EST_read_status (*standard_load_fn_fp)(EST_TokenStream &ts,
				    short **data, int *nsamp, int *nchan, 
				    int *wsize, 
				    int *srate, 
				    EST_sample_type_t *stype, int *bo,
				    int offset, int length);

typedef 
EST_write_status (*standard_save_fn_fp)(FILE *fp,
				    const short *data,
				    int offset, int nsamp,
				    int nchan, int srate,
				    EST_sample_type_t stype, int bo);
				    
  
static 
EST_read_status load_using(standard_load_fn_fp fn,
			   EST_TokenStream &ts,
				  EST_Wave &wv,
				  int rate,
			          EST_sample_type_t stype, int bo, int nchan,
				  int offset, int length)
{
  short *data;
  int nsamp;
  int wsize;
  int srate = rate;

  EST_read_status status =  (*fn)(ts, 
				  &data, &nsamp, &nchan, 
				  &wsize, 
				  &srate, &stype, &bo,
				  offset, length);

  if (status == read_ok)
    {
      wv.values().set_memory(data, 0, nsamp, nchan, TRUE);
      wv.set_sample_rate(srate);
    }

  return status;
}

static
EST_write_status save_using(standard_save_fn_fp fn,
			    FILE *fp, const EST_Wave wv,
			    EST_sample_type_t stype, int bo)
{
EST_write_status status =  (*fn)(fp,
				wv.values().memory(), 
				0, wv.num_samples(), wv.num_channels(),
				wv.sample_rate(),
				stype, bo);

return status; 
}

EST_read_status EST_WaveFile::load_nist(EST_TokenStream &ts,
			  EST_Wave &wv,
			  int rate,
			  EST_sample_type_t stype, int bo, int nchan,
			  int offset, int length)
{
  return load_using(load_wave_nist,
		    ts, wv, rate,
		    stype, bo, nchan,
		    offset, length);
}

EST_write_status EST_WaveFile::save_nist(FILE *fp,
					 const EST_Wave &wv,
					 EST_sample_type_t stype, int bo)
{
  return save_using(save_wave_nist, fp, wv, stype, bo);
}

EST_read_status EST_WaveFile::load_est(EST_TokenStream &ts,
			  EST_Wave &wv,
			  int rate,
			  EST_sample_type_t stype, int bo, int nchan,
			  int offset, int length)
{
    (void) ts;
    return load_using(load_wave_est,
		    ts, wv, rate,
		    stype, bo, nchan,
		    offset, length);

}

EST_write_status EST_WaveFile::save_est(FILE *fp,
					 const EST_Wave &wv,
					 EST_sample_type_t stype, int bo)
{
  return save_using(save_wave_est,
		    fp, wv,
		    stype, bo);

}

EST_read_status EST_WaveFile::load_aiff(EST_TokenStream &ts, 
			  EST_Wave &wv,
			  int rate,
			  EST_sample_type_t stype, int bo, int nchan,
			  int offset, int length)
{
  return load_using(load_wave_aiff,
		    ts, wv,  rate, 
		    stype, bo, nchan,
		    offset, length);
}

EST_write_status EST_WaveFile::save_aiff(FILE *fp,
					 const EST_Wave &wv,
					 EST_sample_type_t stype, int bo)
{
  return save_using(save_wave_aiff, fp, wv, stype, bo);
}


EST_read_status EST_WaveFile::load_riff(EST_TokenStream &ts,
			  EST_Wave &wv,
			  int rate,
			  EST_sample_type_t stype, int bo, int nchan,
			  int offset, int length)
{
  return load_using(load_wave_riff,
		    ts, wv, rate, 
		    stype, bo, nchan,
		    offset, length);
}

EST_write_status EST_WaveFile::save_riff(FILE *fp,
					 const EST_Wave &wv,
					 EST_sample_type_t stype, int bo)
{
  return save_using(save_wave_riff, fp, wv, stype, bo);
}


EST_read_status EST_WaveFile::load_esps(EST_TokenStream &ts, 
			  EST_Wave &wv,
			  int rate,
			  EST_sample_type_t stype, int bo, int nchan,
			  int offset, int length)
{
  return load_using(load_wave_sd,
		    ts, wv, rate, 
		    stype, bo, nchan,
		    offset, length);
}

EST_write_status EST_WaveFile::save_esps(FILE *fp,
					 const EST_Wave &wv,
					 EST_sample_type_t stype, int bo)
{
  return save_using(save_wave_sd,
		    fp, wv,
		    stype, bo);
}


EST_read_status EST_WaveFile::load_audlab(EST_TokenStream &ts, 
			  EST_Wave &wv,
			  int rate,
			  EST_sample_type_t stype, int bo, int nchan,
			  int offset, int length)
{
  return load_using(load_wave_audlab,
		    ts, wv, rate, 
		    stype, bo, nchan,
		    offset, length);
}

EST_write_status EST_WaveFile::save_audlab(FILE *fp,
					 const EST_Wave &wv,
					 EST_sample_type_t stype, int bo)
{
  return save_using(save_wave_audlab, fp, wv, stype, bo);
}


EST_read_status EST_WaveFile::load_snd(EST_TokenStream &ts, 
			  EST_Wave &wv,
			  int rate,
			  EST_sample_type_t stype, int bo, int nchan,
			  int offset, int length)
{
  return load_using(load_wave_snd,
		    ts, wv, rate, 
		    stype, bo, nchan,
		    offset, length);
}

EST_write_status EST_WaveFile::save_snd(FILE *fp,
					const EST_Wave &wv,
					EST_sample_type_t stype, int bo)
{
  return save_using(save_wave_snd, fp, wv, stype, bo);
}


EST_read_status EST_WaveFile::load_raw(EST_TokenStream &ts, 
			  EST_Wave &wv,
			  int rate,
			  EST_sample_type_t stype, int bo, int nchan,
			  int offset, int length)
{
  short *data;
  int nsamp;
  int wsize;
  int srate = rate;

  EST_read_status status =  load_wave_raw(ts, 
					  &data, &nsamp, &nchan, 
					  &wsize, 
					  &srate, &stype, &bo,
					  offset, length,
					  rate, stype, bo, nchan);

  if (status == read_ok)
    {
      wv.values().set_memory(data, 0, nsamp, nchan, TRUE);
      wv.set_sample_rate(srate);
    }

  return status;
}

EST_write_status EST_WaveFile::save_raw(FILE *fp,
					 const EST_Wave &wv,
					 EST_sample_type_t stype, int bo)
{
EST_write_status status =  save_wave_raw(fp,
					 (short *)wv.values().memory(), 
					 0, wv.num_samples(), wv.num_channels(),
					 wv.sample_rate(),
					 stype, bo);
return status; 
}


EST_read_status EST_WaveFile::load_ulaw(EST_TokenStream &ts,
			  EST_Wave &wv,
			  int rate,
			  EST_sample_type_t stype, int bo, int nchan,
			  int offset, int length)
{
  return load_using(load_wave_ulaw,
		    ts, wv, rate, 
		    stype, bo, nchan,
		    offset, length);
}

EST_write_status EST_WaveFile::save_ulaw(FILE *fp,
					 const EST_Wave &wv,
					 EST_sample_type_t stype, int bo)
{
    EST_Wave localwv = wv;
    localwv.resample(8000);
    return save_using(save_wave_ulaw, fp, localwv, stype, bo);
}

static int parse_esps_r_option(EST_String arg, int &offset, int &length)
{
    EST_String s, e;

    if (arg.contains("-"))
    {
	s = arg.before("-");
	e = arg.after("-");
    }
    else if (arg.contains(":"))
    {
	s = arg.before(":");
	e = arg.after(":");
    }
    else
    {
	cerr << "Argument to -r is illformed " << arg << endl;
	return -1;
    }
    
    if (!s.matches(RXint))
    {
	cerr << "First argument to -r must be an integer " << arg << endl;
	return -1;
    }

    offset = atoi(s);
    if (e.contains("+"))
    {	
	e = e.after("+");
	length = atoi(e);
    }
    else
	length = atoi(e) - offset;

    if (length <= 0)
    {
	cerr << "length is negative or zero " << arg << endl;
	return -1;
    }

    return 0;
}

EST_read_status read_wave(EST_Wave &sig, const EST_String &in_file, 
			  EST_Option &al)
{
    char *sr;
    EST_String fname, file_type, sample_type;
    int sample_rate;
    EST_read_status rval;
    int num_channels;
    int offset=0, length=0;
    int bo;
    
    if (in_file == "-")
	fname = stdin_to_file();
    else
	fname = in_file;

    if (al.present("-n"))
	num_channels = al.ival("-n", 0);
    else 
	num_channels = 1;

    if (al.present("-ulaw"))
    {
	al.add_item("-itype","ulaw");
	al.add_item("-f","8000");
    }
    if (al.present("-iswap"))
	al.add_item("-ibo","other");

    if (al.present("-istype"))
	sample_type = al.val("-istype");
    else 
	sample_type = sig.sample_type(); // else default type;

    if (al.present("-itype"))
	file_type = al.val("-itype");  // else default type;
    else
	file_type = "undef";


    if (al.present("-f"))
	sample_rate = al.ival("-f", 0);
    else if ((sr = getenv("NA_PLAY_SAMPLE_RATE")) != NULL)
    {
	sample_rate = atoi(sr);
	cerr << "Warning: no sample rate specified, " <<
	    " using NA_PLAY_SAMPLE_RATE environment variable\n";
    }
    else
    {
      sample_rate = EST_Wave::default_sample_rate;
	if (file_type == "raw")
	    cerr << "Warning: no sample rate specified - using default " << 
		sample_rate << endl;
    }
    
    if (file_type == "ulaw")
    {
	sample_rate = 8000;
	sample_type = "mulaw";
    }
	
    if (al.present("-r")) // only load in part of waveform
    {
	if (parse_esps_r_option(al.val("-r"), offset, length) != 0)
	    return read_error;
    }
    else
	offset = length = 0;

    if (al.present("-iswap"))
	bo = str_to_bo("swap");
    else
	bo = str_to_bo("native");
    if (al.present("-ibo"))   // can override -iswap
	bo = str_to_bo(al.val("-ibo"));

    if (file_type == "" ||file_type == "undef")
      rval = sig.load(fname, offset, length, sample_rate);
    else
      rval = sig.load_file(fname,file_type, sample_rate,
			   sample_type, bo, num_channels, offset, length);

    if ((rval == wrong_format) && (al.present("-basic")))
    {
	// For HTML audio/basic, it seems to mean headered or ulaw 8k
	// so try to load it again as ulaw 8k.
	rval = sig.load_file(fname, "raw", 8000,
			     "mulaw", bo, 1, offset, length);
    }
    if (rval != format_ok)
    {
	if (in_file == "-") unlink(fname);
	cerr << "Cannot recognize file format or cannot access file: \"" << in_file << "\"\n";
	return read_error;
    }

    if (al.present("-start") || al.present("-end") 
	|| al.present("-to") || al.present("-from"))
	extract(sig, al);

    if (in_file == "-") unlink(fname);
    return read_ok;
}

EST_write_status write_wave(EST_Wave &sig, const EST_String &out_file, 
			    EST_Option &al)
{
    EST_String file_type, sample_type;
    int bo;

    if (al.present("-otype"))
	file_type = al.val("-otype");
    else
	file_type = sig.file_type();

    if (al.present("-ostype"))
	sample_type = al.val("-ostype");
    else
	sample_type = "undef";

    if (al.present("-oswap"))
	bo = str_to_bo("swap");
    else
	bo = str_to_bo("native");

    if (al.present("-obo"))   // can over ride -oswap
	bo = str_to_bo(al.val("-obo"));

    if (sample_type == "undef" || sample_type == "")
      sample_type = "short";

    if (sig.save_file(out_file, file_type,
		      sample_type, bo) != write_ok)
    {
	cerr << "Cannot write file: \"" << out_file << "\"\n";
	return write_error;
    }

    return write_ok;
}

EST_String EST_WaveFile::options_short(void)
{
    EST_String s("");
    
    for(int n=0; n< EST_WaveFile::map.n() ; n++)
    {
	const char *nm = EST_WaveFile::map.name(EST_WaveFile::map.token(n));
	
	if (s != "")
	    s += ", ";
	
	s += nm;
	
    }
    return s;
}

EST_String EST_WaveFile::options_supported(void)
{
    EST_String s("Available wave file formats:\n");
    
    for(int n=0; n< EST_WaveFile::map.n() ; n++)
    {
	const char *nm = EST_WaveFile::map.name(EST_WaveFile::map.token(n));
	const char *d = EST_WaveFile::map.info(EST_WaveFile::map.token(n)).description;
	
	s += EST_String::cat("        ", nm, EST_String(" ")*(12-strlen(nm)), d, "\n");
    }
    return s;
}

typedef struct TInfo {
  bool recognise;
  const char *description;
} TInfo;

// note the order here defines the order in which loads are tried.


static 
EST_TValuedEnumDefinition<EST_WaveFileType, const char *, EST_WaveFile::Info> wavefile_names[] =
{
  { wff_none,	{ NULL }, 
    { FALSE, NULL, NULL, "unknown track file type"} },
  { wff_nist,	{ "nist", "timit" }, 
    { TRUE, EST_WaveFile::load_nist,  EST_WaveFile::save_nist, "nist/timit" } },
  { wff_est,	{ "est"}, 
    { TRUE, EST_WaveFile::load_est,  EST_WaveFile::save_est, "est" } },
  { wff_esps,	{ "esps", "sd"}, 
    { TRUE,  EST_WaveFile::load_esps,  EST_WaveFile::save_esps, "esps SD waveform" } },
  { wff_audlab, { "audlab", "vox"}, 
    { TRUE,  EST_WaveFile::load_audlab,  EST_WaveFile::save_audlab, "audlab waveform" } },
  { wff_snd,	{ "snd", "au"}, 
    { TRUE,  EST_WaveFile::load_snd,  EST_WaveFile::save_snd, "Sun snd file" } },
  { wff_aiff,	{ "aiff" }, 
    { TRUE,  EST_WaveFile::load_aiff,  EST_WaveFile::save_aiff, "Apple aiff file" } },
  { wff_riff,	{ "riff", "wav" }, 
    { TRUE,  EST_WaveFile::load_riff,  EST_WaveFile::save_riff, "Microsoft wav/riff file" } },
  { wff_raw,	{ "raw" }, 
    { FALSE,  EST_WaveFile::load_raw,  EST_WaveFile::save_raw, "Headerless File" } },
  { wff_ulaw,	{ "ulaw", "basic" }, 
    { FALSE,  EST_WaveFile::load_ulaw,  EST_WaveFile::save_ulaw, "Headerless 8K ulaw  File" } },
  { wff_none,	{NULL} }
};

EST_TNamedEnumI<EST_WaveFileType, EST_WaveFile::Info> EST_WaveFile::map(wavefile_names);

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TNamedEnum.cc"
template class EST_TNamedEnumI<EST_WaveFileType, EST_WaveFile::Info>;
template class EST_TValuedEnumI<EST_WaveFileType, const char *, 
EST_WaveFile::Info>;

#endif
