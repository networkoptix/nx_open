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
/*                        Author :  Paul Taylor                          */
/*                        Date   :  August 1995                          */
/*-----------------------------------------------------------------------*/
/*                   File I/O functions for EST_Track class              */
/*                                                                       */
/*=======================================================================*/
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <time.h>
#include "EST_unix.h"
#include "EST_types.h"
#include "EST_Track.h"
#include "EST_track_aux.h"
#include "EST_TrackMap.h"
#include "EST_cutils.h"
#include "EST_Token.h"
#include "EST_TList.h"
#include "EST_string_aux.h"
#include "EST_walloc.h"
#include "EST_TrackFile.h"
#include "EST_FileType.h"
#include "EST_WaveFile.h"
#include "EST_wave_utils.h"

// size of locally allocated buffer. If more channels than this we have to
// call new

#define NEARLY_ZERO 0.00001

#define REASONABLE_FRAME_SIZE (20)
#define UNREASONABLE_FRAME_SIZE (80)

#if 0
static const char *NIST_SIG = "NIST_1A\n   1024\n";
static const char *NIST_END_SIG = "end_head\n";
#define NIST_HDR_SIZE 1024
// default for tracks is the standard EMA sample rate
static int def_load_sample_rate = 500;

#endif

// some functions written for reading NIST headered waveform files,
// but useful here.
int nist_get_param_int(char *hdr, char *field, int def_val);
char *nist_get_param_str(char *hdr, char *field, char *def_val);
const char *sample_type_to_nist(enum EST_sample_type_t sample_type);
enum EST_sample_type_t nist_to_sample_type(char *type);

EST_read_status read_est_header(EST_TokenStream &ts, EST_Features &hinfo, 
				bool &ascii, EST_EstFileType &t);

EST_read_status EST_TrackFile::load_esps(const EST_String filename, EST_Track &tr, float ishift, float startt)
{
  (void)ishift;
  (void)startt;

    float **tt;
    float fsize;
    char **fields;
    int num_points, num_fields, num_values;
    int i,j;
    EST_read_status  r_val;
    short fixed;
    int first_channel=0;

    r_val = get_track_esps(filename, &fields, &tt, &fsize, &num_points, 
			   &num_values, &fixed);
    if (r_val == misc_read_error)
    {
	cerr << "Error reading ESPS file " << filename << endl;
	return misc_read_error;
    }
    else if (r_val == wrong_format)
	return wrong_format;

    num_fields = num_values;
    if (!fixed)
      {
	--num_fields;
	++first_channel;
      }

    tr.resize(num_points,num_fields);
    tr.fill_time(fsize);

    for (i = 0; i < num_points; ++i)
    {
      for (j = 0; j < num_fields; ++j)
	tr.a(i, j) = tt[i][j+first_channel];
      tr.set_value(i);
      if (!fixed)
	tr.t(i) = tt[i][0];
    }

    for (i = 0; i < num_fields; ++i)
	tr.set_channel_name(fields[i+first_channel], i);


    // REORG not sure what these should be -- awb
    tr.set_single_break(false);
    tr.set_equal_space(true);

    // get_track_esps allocs all the memory, we therefore need to release it
    for (i = 0; i < num_values; ++i)
	wfree(fields[i]);
    wfree(fields);
    for (i = 0; i < num_values; ++i)
	wfree(tt[i]);
    wfree(tt);

    tr.set_file_type(tff_esps);
    tr.set_name(filename);
 
   if (tr.channel_name(0) == "F0")
       espsf0_to_track(tr);

    return format_ok;
}

EST_read_status EST_TrackFile::load_ascii(const EST_String filename, EST_Track &tr, float ishift, float startt)
{
    (void)startt;

    EST_TokenStream ts, tt;
    EST_StrList sl;

    int i, j, n_rows, n_cols=0;
    EST_String t;
    EST_Litem *p;
    
    if (((filename == "-") ? ts.open(cin) : ts.open(filename)) != 0)
    {
	cerr << "Can't open track file " << filename << endl;
	return misc_read_error;
    }
    // set up the character constant values for this stream
    ts.set_SingleCharSymbols(";");
    
    if (ishift < NEARLY_ZERO)
    {
      cerr<<
	  "Error: Frame spacing must be specified (or apparent frame shift nearly zero)\n";
      return misc_read_error;
    }

    // first read in as list

    for (n_rows = 0; !ts.eof(); ++n_rows)
	sl.append(ts.get_upto_eoln().string());

    if (n_rows > 0)
    {
	tt.open_string(sl.first());
	for (n_cols = 0; !tt.eof(); ++n_cols)
	    tt.get().string();
    }

    // resize track and copy values in
    tr.resize(n_rows, n_cols);

    for (p = sl.head(), i = 0; p != 0; ++i, p = p->next())
    {
        bool ok;
	tt.open_string(sl(p));
	for (j = 0; !tt.eof(); ++j)
	  tr.a(i, j) = tt.get().Float(ok);
	if (j != n_cols)
	{
	    cerr << "Wrong number of points in row " << i << endl;
	    cerr << "Expected " << n_cols << " got " << j << endl;
	    return misc_read_error;
	}
    }

    tr.fill_time(ishift);
    tr.set_single_break(FALSE);
    tr.set_equal_space(TRUE);
    tr.set_file_type(tff_ascii);
    tr.set_name(filename);

    return format_ok;
}

EST_read_status EST_TrackFile::load_xgraph(const EST_String filename, EST_Track &tr, float ishift, float startt)
{
  (void)ishift;
  (void)startt;

    EST_TokenStream ts, tt;
    EST_StrList sl;
    // const float NEARLY_ZERO = 0.001;
    int i, j, n_rows, n_cols;
    EST_String t;
    EST_Litem *p;
    
    if (((filename == "-") ? ts.open(cin) : ts.open(filename)) != 0)
    {
	cerr << "Can't open track file " << filename << endl;
	return misc_read_error;
    }
    // set up the character constant values for this stream
    ts.set_SingleCharSymbols(";");
    
    // first read in as list

    for (n_rows = 0; !ts.eof(); ++n_rows)
	sl.append(ts.get_upto_eoln().string());

    tt.open_string(sl.first());
    for (n_cols = 0; !tt.eof(); ++n_cols)
    	tt.get().string();

    --n_cols; // first column is time marks

    // resize track and copy values in
    tr.resize(n_rows, n_cols);

    for (p = sl.head(), i = 0; p != 0; ++i, p = p->next())
    {
        bool ok; 
	tt.open_string(sl(p));
	tr.t(i) = tt.get().Float(ok);
	for (j = 0; !tt.eof(); ++j)
	    tr.a(i, j) = tt.get().Float(ok);
	if (j != n_cols)
	{
	    cerr << "Wrong number of points in row " << i << endl;
	    cerr << "Expected " << n_cols << " got " << j << endl;
	    return misc_read_error;
	}
    }

    tr.set_single_break(FALSE);
    tr.set_equal_space(TRUE);
    tr.set_file_type(tff_xgraph);
    tr.set_name(filename);

    return format_ok;
}

EST_read_status EST_TrackFile::load_xmg(const EST_String filename, EST_Track &tr, float ishift, float startt)
{
  (void)ishift;
  (void)startt;

    EST_TokenStream ts;
    EST_StrList sl;
    int i, n, sr;
    EST_String t, k, v;
    EST_Litem *p;
    
    if (((filename == "-") ? ts.open(cin) : ts.open(filename)) != 0)
    {
	cerr << "Can't open track file " << filename << endl;
	return misc_read_error;
    }
    // set up the character constant values for this stream
    ts.set_SingleCharSymbols(";");

    if (ts.peek().string() != "XAO1")
	return wrong_format;

    ts.get().string();

    while ((!ts.eof()) && (ts.peek().string() != "\014"))
    {
	k = ts.get().string();
	v = ts.get().string();
	if (k == "Freq")
	    sr = v.Int() * 1000;
	else if (k == "YMin")
	  /* tr.amin = atof(v) */;
	else if (k == "YMax")
	  /*tr.amax = atof(v) */;
    }

    if (ts.eof())
    {
	cerr << "Unexpected end of file in reading xmg header\n";
	return misc_read_error;
    }
    ts.get().string(); // read control L
    ts.get_upto_eoln().string(); // read until end of header

    // read in lines to a list
    for (n = 0; !ts.eof(); ++n)
	sl.append(ts.get_upto_eoln().string());
    
    // note the track size is total number of points *and* breaks
    tr.resize(n, 1 );  // REORG - fix this for multi channel work

    for (p = sl.head(), i = 0; p != 0; ++i, p = p->next())
    {
        bool ok;
	ts.open_string(sl(p));
	if (ts.peek().string() != "=")
	{
	    tr.t(i) = ts.get().Float(ok) / 1000.0;
	    tr.a(i) = ts.get().Float(ok);
	}
	else
	{
	    ts.get().string();
	    tr.set_break(i);
	}
    }

    tr.set_single_break(TRUE);
    tr.set_equal_space(FALSE);
    tr.set_file_type(tff_xmg);
    tr.set_name(filename);

    return format_ok;
}

EST_read_status EST_TrackFile::load_est(const EST_String filename, 
					EST_Track &tr, float ishift, float startt)
{
    EST_TokenStream ts;
    EST_read_status r;
    
    if (((filename == "-") ? ts.open(cin) : ts.open(filename)) != 0)
    {
	cerr << "Can't open track file " << filename << endl;
	return misc_read_error;
    }
    // set up the character constant values for this stream
    ts.set_SingleCharSymbols(";");
    tr.set_name(filename);
    r = load_est_ts(ts, tr, ishift, startt);

    if ((r == format_ok) && (!ts.eof()))
    {
	cerr << "Not end of file, but expected it\n";
	return misc_read_error;
    }
    else
	return r;
}

static float get_float(EST_TokenStream &ts,int swap)
{
    float f;
    ts.fread(&f,4,1);
    if (swap) swapfloat(&f);
    return f;
}

EST_read_status EST_TrackFile::load_est_ts(EST_TokenStream &ts,
					EST_Track &tr, float ishift, float startt)
{
    (void)ishift;
    (void)startt;
    int i, j;
    int num_frames, num_channels, num_aux_channels;
    EST_Features hinfo;
    EST_EstFileType t;
    EST_String v;
    bool ascii;
    bool breaks;
    bool eq_space;
    EST_read_status r;
    int swap;

    if ((r = read_est_header(ts, hinfo, ascii, t)) != format_ok)
	return r;
    if (t != est_file_track)
	return misc_read_error;

    breaks = hinfo.present("BreaksPresent") ? true : false;
    eq_space = false;
    if ((hinfo.present("EqualSpace")) &&
	((hinfo.S("EqualSpace") == "true") ||
	 (hinfo.S("EqualSpace") == "1")))
	eq_space = true;

    num_frames = hinfo.I("NumFrames");
    num_channels = hinfo.I("NumChannels");
    num_aux_channels = hinfo.I("NumAuxChannels", 0);
    tr.resize(num_frames, num_channels);

    hinfo.remove("NumFrames");
    hinfo.remove("EqualSpace");
    hinfo.remove("NumChannels");
    hinfo.remove("BreaksPresent");
    hinfo.remove("DataType");
    if (hinfo.present("NumAuxChannels"))
      hinfo.remove("NumAuxChannels");

    EST_String strn, cname;
    
    EST_Features::Entries p, c;
    EST_StrList ch_map;

    for (p.begin(hinfo); p;)
      {
	c = p++;

	if (c->k.contains("Aux_Channel_"))
	{
	      ch_map.append(c->v.String());
	      hinfo.remove(c->k);
	}
	else if (c->k.contains("Channel_"))
	  {
	    tr.set_channel_name(c->v.String(),
				c->k.after("Channel_").Int());
	    hinfo.remove(c->k);
	  }
      }

    tr.resize_aux(ch_map);

//    tr.create_map();

//    if (((hinfo.S("ByteOrder", "") == "01") ? bo_little : bo_big) 

    if (!hinfo.present("ByteOrder"))
	swap = FALSE;  // ascii or not there for some reason
    else if (((hinfo.S("ByteOrder") == "01") ? bo_little : bo_big) 
	!= EST_NATIVE_BO)
	swap = TRUE;
    else
	swap = FALSE;
	    
    const int BINARY_CHANNEL_BUFFER_SIZE=1024;
    float *frame=0; 
    float frame_buffer[BINARY_CHANNEL_BUFFER_SIZE];
    if( !ascii )
    {
      if( num_channels > BINARY_CHANNEL_BUFFER_SIZE )
	frame = new float[num_channels];
      else
	frame = frame_buffer;
    }

    // there are too many ifs here
    for (i = 0; i < num_frames; ++i)
    {
        bool ok;

	// Read Times
	if (ascii)
	{
	    if (ts.eof())
	    {
		cerr << "unexpected end of file when looking for " << num_frames-i << " more frame(s)" << endl;
		return misc_read_error;
	    }
	    tr.t(i) = ts.get().Float(ok);
	    if (!ok)
	      	return misc_read_error;
	}
	else
	    tr.t(i) = get_float(ts,swap);

	// Read Breaks
	if (breaks)
	{
	    if (ascii)
	    {
		v = ts.get().string();
		if (v == "0") 
		    tr.set_break(i);
		else
		    tr.set_value(i);
	    }
	    else
	    {
		if (get_float(ts,swap) == 0.0)
		    tr.set_break(i);
		else
		    tr.set_value(i);
	    }
	}
	else
	    tr.set_value(i);	

	// Read Channels
// 	for (j = 0; j < num_channels; ++j)
// 	{
// 	    if(ascii)
// 	      {
// 		tr.a(i, j) = ts.get().Float(ok);
// 		if (!ok)
// 		  return misc_read_error;
// 	      }
// 	    else
// 		tr.a(i,j) = get_float(ts,swap);
// 	}

	if( ascii ){
	  for (j = 0; j < num_channels; ++j){
	    tr.a(i, j) = ts.get().Float(ok);
	    if (!ok)
	      return misc_read_error;
	  }
	}
	else{
	  ts.fread( frame, sizeof(float), num_channels );
	  if( swap )
	    for( j=0; j<num_channels; ++j ){
	      swapfloat( &frame[j] );
	      tr.a(i,j) = frame[j];
	    }
	  else
	    for( j=0; j<num_channels; ++j )
	      tr.a(i,j) = frame[j];
	}
	

	// Read aux Channels
	for (j = 0; j < tr.num_aux_channels(); ++j)
	{
	    if (ascii)
	      {
		tr.aux(i, j) = ts.get().string();
		if (!ok)
		  return misc_read_error;
	      }
	    else
	    {
		cerr << "Warning: Aux Channel reading not yet implemented";
		cerr << "for binary tracks\n";
	    }
	}
    }
    
    if( !ascii )
      if( frame != frame_buffer )
	delete [] frame;

    // copy header info into track
    tr.f_set(hinfo);

    tr.set_single_break(FALSE);
    tr.set_equal_space(eq_space);

    if(ascii)
	tr.set_file_type(tff_est_ascii);
    else
	tr.set_file_type(tff_est_binary);
    
    return format_ok;
}

EST_read_status load_snns_res(const EST_String filename, EST_Track &tr, 
			      float ishift, float startt)
{
    (void)startt;
  
    EST_TokenStream ts, str;
    EST_StrList sl;
    int i, j;
    EST_String t, k, v;

    if (ishift < NEARLY_ZERO)
    {
	cerr<<
	    "Error: Frame spacing must be specified (or apparent frame shift nearly zero)\n";
	return misc_read_error;
    }
    
    if (((filename == "-") ? ts.open(cin) : ts.open(filename)) != 0)
    {
	cerr << "Can't open track file " << filename << endl;
	return misc_read_error;
    }
    
    if (ts.get().string() != "SNNS")
	return wrong_format;
    if (ts.get().string() != "result")
	return wrong_format;
    
    ts.get_upto_eoln();		// SNNS bit
    ts.get_upto_eoln();		// Time info
    
    int num_frames=0, num_channels=0;
    int teaching = 0;
    
    while (1)
    {
	t = (EST_String)ts.get_upto_eoln();
	//	cout << "t=" << t << endl;
	if (t.contains("teaching output included"))
	    teaching = 1;
	if (!t.contains(":"))
	    break;
	str.open_string(t);
	k = (EST_String)str.get_upto(":");
	v = (EST_String)str.get_upto_eoln();
	if (k == "No. of output units")
	    num_channels = v.Int();
	if (k == "No. of patterns")
	    num_frames = v.Int();
	
	//	cout << "key " << k << endl;
	//	cout << "val " << v << endl;
    }
    
    //    cout << "num_frames = " << num_frames << endl;
    //    cout << "num_channels = " << num_channels << endl;
    tr.resize(num_frames, num_channels);
    //    cout << "peek" << ts.peek().string() << endl;
    //    cout << "teaching " << teaching << endl;
    
    for (i = 0; (!ts.eof()) && (i < num_frames);)
	//    for (i = 0; i < 10; ++i)
    {
	if (ts.peek().string().contains("#")) // comment
	{
	    ts.get_upto_eoln();
	    continue;
	}
	if (teaching)		// get rid of teaching patterns
	    for (j = 0; j < num_channels; ++j)
		ts.get().string();
	
	//	cout << "i = " << i << " t = " << ts.peek().string() << endl;

	bool ok;

	for (j = 0; j < num_channels; ++j)
	    tr.a(i, j) = ts.get().Float(ok);
	
	++i;
    }
    
    tr.fill_time(ishift);
    tr.set_single_break(FALSE);
    tr.set_equal_space(TRUE);
    tr.set_file_type(tff_snns);
    tr.set_name(filename);
    
    return format_ok;
}

EST_write_status EST_TrackFile::save_esps(const EST_String filename, EST_Track tr)
{
    EST_write_status rc;
    int i, j;
    float shift;
    bool include_time;
    int extra_channels=0;
    
    EST_Track &track_tosave = tr;
    
    if (filename == "-")
    {
	cerr << "Output to stdout not available for ESPS file types:";
	cerr << "no output written\n";
	return write_fail;
    }
    
    if ((include_time = (track_tosave.equal_space() != TRUE)))
    {
	shift = EST_Track::default_frame_shift;
	extra_channels++;
    }
    else 
	shift = track_tosave.shift();
    
    track_tosave.change_type(0.0,FALSE);
    
    float **a = new float*[track_tosave.num_frames()];
    // pity we need to copy it
    for (i=0; i < track_tosave.num_frames(); i++)
    {
	a[i] = new float[track_tosave.num_channels() + extra_channels];
	
	if (include_time)
	    a[i][0] = track_tosave.t(i);
	
	for (j=0; j < track_tosave.num_channels(); j++)
	    a[i][j + extra_channels] = track_tosave.a(i,j);
    }
    
    char **f_names = new char*[track_tosave.num_channels() + extra_channels];
    for (i=0; i < track_tosave.num_channels(); i++)
    {
	// cout << "field " << i << "is '" << track_tosave.field_name(i) << "'\n";
	f_names[i + extra_channels] = wstrdup(track_tosave.channel_name(i, esps_channel_names, 0));
    }
    
    if (include_time)
	f_names[0] = wstrdup("EST_TIME");
    
    rc = put_track_esps(filename, f_names,
			a, shift, 1.0/shift, 
			track_tosave.num_channels() + extra_channels,
			track_tosave.num_frames(),
			!include_time);
    
    for (i=0; i < track_tosave.num_frames(); i ++)
	delete [] a[i];
    delete [] a;
    for (i=0; i < track_tosave.num_channels()+extra_channels; i++)
	delete [] f_names[i];
    delete [] f_names;
    
    return rc;
}

EST_write_status EST_TrackFile::save_est_ts(FILE *fp, EST_Track tr)
{
    int i, j;

    fprintf(fp, "EST_File Track\n"); // EST header identifier.
    fprintf(fp, "DataType ascii\n");
    fprintf(fp, "NumFrames %d\n", tr.num_frames());
    fprintf(fp, "NumChannels %d\n", tr.num_channels());
    fprintf(fp, "NumAuxChannels %d\n", tr.num_aux_channels());
    fprintf(fp, "EqualSpace %d\n",tr.equal_space());

    fprintf(fp, "BreaksPresent true\n");
    for (i = 0; i < tr.num_channels(); ++i)
	fprintf(fp, "Channel_%d %s\n", i, (const char *)(tr.channel_name(i)));

    for (i = 0; i < tr.num_aux_channels(); ++i)
	fprintf(fp, "Aux_Channel_%d %s\n", i, 
		(const char *)(tr.aux_channel_name(i)));

    EST_Featured::FeatEntries p;

    for (p.begin(tr); p; ++p)
	fprintf(fp, "%s %s\n", (const char *)p->k,
		(const char *) p->v.String());

    fprintf(fp, "EST_Header_End\n");
    
    for (i = 0; i < tr.num_frames(); ++i)
    {
	fprintf(fp, "%f\t", tr.t(i));
	fprintf(fp, "%s\t", (char *)(tr.val(i) ? "1 " : "0 "));
	for (j = 0; j < tr.num_channels(); ++j)
	    fprintf(fp, "%f ", tr.a_no_check(i, j));
	for (j = 0; j < tr.num_aux_channels(); ++j)
	    fprintf(fp, "%s ", (const char *)tr.aux(i, j).string());
	fprintf(fp, "\n");
    }
    return write_ok;
}

EST_write_status EST_TrackFile::save_est_ascii(const EST_String filename, 
					       EST_Track tr)
{
    FILE *fd;
    EST_write_status r;

    if (filename == "-")
	fd = stdout;
    else if ((fd = fopen(filename,"wb")) == NULL)
	return write_fail;

    r = save_est_ts(fd,tr);
    
    if (fd != stdout)
	fclose(fd);
    return r;
}

EST_write_status EST_TrackFile::save_est_binary(const EST_String filename, EST_Track tr)
{
    FILE *fd;
    EST_write_status r;

    if (filename == "-")
	fd = stdout;
    else if ((fd = fopen(filename,"wb")) == NULL)
	return write_fail;

    r = save_est_binary_ts(fd,tr);
    
    if (fd != stdout)
	fclose(fd);
    return r;

}

EST_write_status EST_TrackFile::save_est_binary_ts(FILE *fp, EST_Track tr)
{
    int i,j;

    // This should be made optional
    bool breaks = TRUE;

    fprintf(fp, "EST_File Track\n");
    fprintf(fp, "DataType binary\n");
    fprintf(fp, "ByteOrder %s\n", ((EST_NATIVE_BO == bo_big) ? "10" : "01"));
    fprintf(fp, "NumFrames %d\n", tr.num_frames());
    fprintf(fp, "NumChannels %d\n",tr.num_channels());
    fprintf(fp, "EqualSpace %d\n",tr.equal_space());
    if(breaks)
	fprintf(fp, "BreaksPresent true\n");
    fprintf(fp, "CommentChar ;\n\n");
    for (i = 0; i < tr.num_channels(); ++i)
	fprintf(fp, "Channel_%d %s\n",i,tr.channel_name(i).str());
    fprintf(fp, "EST_Header_End\n");

    for (i = 0; i < tr.num_frames(); ++i)
    {
	// time
	if((int)fwrite(&tr.t(i),4,1,fp) != 1)
	    return misc_write_error;

	// break marker
	if (breaks)
	{
	    float bm = (tr.val(i) ? 1 : 0);
	    if((int)fwrite(&bm,4,1,fp) != 1)
		return misc_write_error;
	}
	// data - restricted to floats at this time
	for (j = 0; j < tr.num_channels(); ++j)
	    if((int)fwrite(&tr.a_no_check(i, j),4,1,fp) != 1)
		return misc_write_error;
	
    }
    return write_ok;
}

EST_write_status EST_TrackFile::save_ascii(const EST_String filename, EST_Track tr)
{
    
    if (tr.equal_space() == TRUE)
	tr.change_type(0.0, FALSE);
    
    ostream *outf;
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))
	return write_fail;
    
    outf->precision(5);
    outf->setf(ios::fixed, ios::floatfield);
    outf->width(8);
    
    for (int i = 0; i < tr.num_frames(); ++i)
    {
	for (int j = 0; j < tr.num_channels(); ++j)
	    *outf << tr.a(i, j) << " ";
	*outf << endl;
    }
    
    if (outf != &cout)
	delete outf;
    
    return write_ok;
}

EST_write_status EST_TrackFile::save_xgraph(const EST_String filename, EST_Track tr)
{
    
    ostream *outf;
    
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))
	return write_fail;
    
    tr.change_type(0.0, TRUE);
    
    for  (int j = 0; j < tr.num_channels(); ++j)
    {
	*outf << "\""<< tr.channel_name(j) << "\"\n";    
	for (int i = 0; i < tr.num_frames(); ++i)
	    if (tr.val(i))
		*outf << tr.t(i) << "\t" << tr.a(i, j) << endl;
	    else
		*outf << "move  ";
    }
    if (outf != &cout)
	delete outf;
    
    return write_ok;
}

EST_write_status save_snns_pat(const EST_String filename, 
			       EST_TrackList &inpat, EST_TrackList &outpat)
{    
    ostream *outf;
    int num_inputs, num_outputs, num_pats, i;
    EST_Litem *pi, *po;
    
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))
	return write_fail;
    
    num_pats = 0;
    for (pi = inpat.head(); pi ; pi = pi->next())
	num_pats += inpat(pi).num_frames();
    
    *outf << "SNNS pattern definition file V3.2\n";
    
    time_t thetime = time(0);
    char *date = ctime(&thetime);
    
    *outf << date;
    *outf << endl;
    
    num_inputs = inpat.first().num_channels();
    num_outputs = outpat.first().num_channels();
    
    *outf << "No. of patterns : " << num_pats << endl;
    *outf << "No. of input units : "<< num_inputs << endl;
    *outf << "No. of output units : "<<  num_outputs << endl;
    *outf << endl << endl;
    
    for (pi = inpat.head(), po = outpat.head(); pi ; 
	 pi = pi->next(), po = po->next())
    {
	if (inpat(pi).num_frames() != outpat(pi).num_frames())
	{
	    cerr << "Error: Input pattern has " << inpat(pi).num_frames() 
		<< " output pattern has " << outpat(pi).num_frames() << endl;
	    if (outf != &cout)
		delete outf;
	    return misc_write_error;
	}
	for (i = 0; i < inpat(pi).num_frames(); ++i)
	{
	    int j;
	    *outf << "#Input pattern " << (i + 1) << ":\n";
	    for  (j = 0; j < inpat(pi).num_channels(); ++j)
		*outf << inpat(pi).a(i, j) << " ";
	    *outf << endl;
	    *outf << "#Output pattern " << (i + 1) << ":\n";
	    for  (j = 0; j < outpat(po).num_channels(); ++j)
		*outf << outpat(po).a(i, j) << " ";
	    *outf << endl;
	}
    }
    if (outf != &cout)
	delete outf;
    
    return write_ok;
}

/*
   EST_write_status EST_TrackFile::save_snns_pat(const EST_String filename, 
   EST_TrackList &trlist)
   {
   ostream *outf;
   int num_inputs, num_outputs, i;
   EST_Litem *p;
   
   if (filename == "-")
   outf = &cout;
   else
   outf = new ofstream(filename);
   
   if (!(*outf))
   return write_fail;
   
   *outf << "SNNS pattern definition file V3.2\n";
   
   char *date;
   date = ctime(clock());
   
   *cout << date << endl;
   
   *cout << endl << endl;
   
   num_inputs = tr.first.num_channels();
   num_outputs = tr.first.num_channels();
   
   *cout << "No. of patterns : " << tr.size() << endl;
   *cout << "No. of input units : "<< num_inputs << endl;
   *cout << "No. of output units : "<<  num_outputs << endl;
   
   for (i = 0, p = trlist.head(); p ; p = p->next(), ++i)
   {
   *outf << "#Input pattern " << i << ":\n";
   for  (int j = 0; j < num_inputs; ++j)
   *outf << << trlist(p)._name(j) << "\"\n";    
   for (int i = 0; i < tr.num_frames(); ++i)
   if (tr.val(i))
   *outf << tr.t(i) << "\t" << tr.a(i, j) << endl;
   else
   *outf << "move  ";
   }
   if (outf != &cout)
   delete outf;
   
   return write_ok;
   }
   */

EST_write_status EST_TrackFile::save_xmg(const EST_String filename, EST_Track tr)
{
    ostream *outf;
    int i, j;
    // float min, max;
    int sr = 16000;		// REORG - fixed sample rate until xmg is fixed
    
    // this takes care of rescaling
    tr.change_type(0.0, TRUE);
    
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))
	return write_fail;
    
    outf->precision(5);
    outf->setf(ios::fixed, ios::floatfield);
    outf->width(8);
    
/*    min = max = tr.a(0);
    for (i = 0; i < tr.num_frames(); ++i)
    {
	if (tr.a(i) > max) max = tr.a(i);
	if (tr.a(i) < min) min = tr.a(i);
    }
*/
    *outf << "XAO1\n\n";	// xmg header identifier.
    *outf << "LineType        segments \n";
    *outf << "LineStyle       solid \n";
    *outf << "LineWidth       0 \n";
    *outf << "Freq " << sr / 1000 << endl; // a REAL pain!
    *outf << "Format  Binary \n";
    // *outf << "YMin    " << ((tr.amin != 0.0) ? tr.amin : min) << endl;
    // *outf << "YMax    " << ((tr.amax != 0.0) ? tr.amax : max) << endl;
    /* if (tr.color != "")
	*outf << "LineColor  " << tr.color << endl;
	*/
    *outf << char(12) << "\n";	// control L character
    
    //    rm_excess_breaks();
    //    rm_trailing_breaks();
    for (i = 0; i < tr.num_frames(); ++i)
	if (tr.val(i))
	{
	    *outf << tr.ms_t(i) << "\t";
	    for (j = 0; j < tr.num_channels(); ++j)
		*outf <<tr.a(i, j) << " ";
	    *outf << endl;
	}
	else
	    *outf << "=\n";
    if (outf != &cout)
	delete outf;
    
    return write_ok;
}

static EST_write_status save_htk_as(const EST_String filename, 
				    EST_Track &orig, 
				    int use_type)
{
    // file format is a 12 byte header
    // followed by data

    // the data is generally floats, except for DISCRETE
    // where it is 2 byte ints

    float s;

    EST_Track track;
    int type;
    int file_num_channels = orig.num_channels();
    
    if (orig.f_String("contour_type","none") == "ct_lpc")
	type = track_to_htk_lpc(orig, track);
    else
    {
	track = orig;
	type = use_type;
    }
    
    if (track.equal_space() != TRUE)
    {
	track.change_type(0.0, FALSE);
	s = rint((HTK_UNITS_PER_SECOND * EST_Track::default_frame_shift/1000.0)/10.0) * 10.0;
	type |= HTK_EST_PS;
	file_num_channels += 1;
    }
    else
    {
	track.change_type(0.0, FALSE);
	s = rint((HTK_UNITS_PER_SECOND * track.shift())/10.0) * 10.0;
    }

    // hkt files need to be big_endian irrespective of hardware. The
    // code here was obviously only ever ran on a Sun.  I've tried to
    // fix this and it seems to work with floats, don't have data to
    // check with shorts though. (Rob, March 2004)

    struct htk_header header;

    header.num_samps = (EST_BIG_ENDIAN ? track.num_frames()
			: SWAPINT(track.num_frames()));


    header.samp_period = (EST_BIG_ENDIAN ? (long) s : SWAPINT((long) s));
    if(use_type == HTK_DISCRETE)
      header.samp_size = (EST_BIG_ENDIAN ? sizeof(short) :
			  SWAPSHORT(sizeof(short)));
    else
      header.samp_size = (EST_BIG_ENDIAN ? (sizeof(float) * file_num_channels) :
			  SWAPSHORT((sizeof(float) * file_num_channels)));
			  
    header.samp_type = EST_BIG_ENDIAN ? type : SWAPSHORT(type);	

    int i, j;
    FILE *outf;
    if (filename == "-")
	outf = stdout;
    else if ((outf = fopen(filename,"wb")) == NULL)
    {
	cerr << "save_htk: cannot open file \"" << filename << 
	    "\" for writing." << endl;
	return misc_write_error;
    }
    
    // write the header
    fwrite((char*)&(header.num_samps), 1, sizeof(header.num_samps), outf);
    fwrite((char*)&(header.samp_period), 1, sizeof(header.samp_period), outf);
    fwrite((char*)&(header.samp_size), 1, sizeof(header.samp_size), outf);
    fwrite((char*)&(header.samp_type), 1, sizeof(header.samp_type), outf);

    // write the data
    if(use_type == HTK_DISCRETE)
    {
	if(track.num_channels() < 1)
	{
	    cerr << "No data to write as HTK_DISCRETE !" << endl;
	}
	else 
	{
	    if(track.num_channels() > 1)
	    {
		cerr << "Warning: multiple channel track being written" << endl;
		cerr << "         as discrete will only save channel 0 !" << endl;
	    }
	    for (i = 0; i < track.num_frames(); ++i)
	    {
		short tempshort = (EST_BIG_ENDIAN ? (short)(track.a(i, 0)) :
				   SWAPSHORT((short)(track.a(i, 0)))) ;
		fwrite((unsigned char*) &tempshort, 1, sizeof(short), outf);
	    }
	}
    }
    else // not HTK_DISCRETE
	for (i = 0; i < track.num_frames(); ++i)	
	{
	    if ((type & HTK_EST_PS) != 0)
	      {
		if(!EST_BIG_ENDIAN)
		  swapfloat(&(track.t(i)));
		fwrite((unsigned char*) &(track.t(i)), 1, sizeof(float), outf);
	      }
	    for (j = 0; j < track.num_channels(); ++j)
	      {
		if(!EST_BIG_ENDIAN)
		  swapfloat(&(track.a(i,j)));
		fwrite((unsigned char*) &(track.a(i, j)), 1, sizeof(float), outf);
	      }
	}
    
    if (outf != stdout)
	fclose(outf);
    
    return write_ok;
}

static int htk_sane_header(htk_header *htk)
{
    return htk->num_samps > 0 &&
	htk->samp_period > 0 &&
	    htk->samp_size > 0 &&
		htk->samp_size < (short)(UNREASONABLE_FRAME_SIZE * sizeof(float));
}

static int htk_swapped_header(htk_header *header)
{
    //  Tries to guess if the header is swapped.  If so it
    //  swaps the contents and returns TRUE, other returns FALSE
    //  HTK doesn't have a magic number so we need heuristics to
    //  guess when its byte swapped
    
    if (htk_sane_header(header))
	return 0;
    
    header->num_samps = SWAPINT(header->num_samps);
    header->samp_period = SWAPINT(header->samp_period);
    header->samp_size = SWAPSHORT(header->samp_size);
    header->samp_type = SWAPSHORT(header->samp_type);
    
    if (htk_sane_header(header))
	return 1;
    
    return -1;
    
}

EST_write_status EST_TrackFile::save_htk(const EST_String filename, EST_Track tmp)
{
    return save_htk_as(filename, tmp, HTK_FBANK);
}

EST_write_status EST_TrackFile::save_htk_fbank(const EST_String filename, EST_Track tmp)
{
    return save_htk_as(filename, tmp, HTK_FBANK);
}

EST_write_status EST_TrackFile::save_htk_mfcc(const EST_String filename, EST_Track tmp)
{
    return save_htk_as(filename, tmp, HTK_MFCC);
}

EST_write_status EST_TrackFile::save_htk_mfcc_e(const EST_String filename, EST_Track tmp)
{
    return save_htk_as(filename, tmp, HTK_MFCC | HTK_ENERGY);
}

EST_write_status EST_TrackFile::save_htk_user(const EST_String filename, EST_Track tmp)
{
    return save_htk_as(filename, tmp, HTK_USER);
}

EST_write_status EST_TrackFile::save_htk_discrete(const EST_String filename, EST_Track tmp)
{
    return save_htk_as(filename, tmp, HTK_DISCRETE);
}


static EST_read_status load_ema_internal(const EST_String filename, EST_Track &tmp, float ishift, float startt, bool swap)
{
    (void)ishift;
    (void)startt;
    
    int i, j, k, nframes, new_order;
    EST_TVector<short> file_data;
    int sample_width, data_length;
    float shift;
    FILE *fp;
    
    if ((fp = fopen(filename, "rb")) == NULL)
    {
	cerr << "EST_Track load: couldn't open EST_Track input file" << endl;
	return misc_read_error;
    }
    
    fseek(fp, 0, SEEK_END);
    sample_width = 2;
    data_length = ftell(fp)/sample_width;
    new_order = 10;
    nframes = data_length /new_order;
    shift = 0.002;
    
    cout << "d length: " << data_length << " nfr " << nframes << endl;
    
    tmp.resize(nframes, new_order);
    tmp.fill_time(shift);
    tmp.set_equal_space(TRUE);

    file_data.resize(data_length);
    
    fseek(fp, 0, SEEK_SET);
    
    if ((int)fread(file_data.memory(), sample_width, data_length, fp) != data_length)
    {
	fclose(fp);
	return misc_read_error;
    }

    if (swap)
      swap_bytes_short(file_data.memory(), data_length);
    
    for (i = k = 0; i < nframes; ++i)
	for (j = 0; j < new_order; ++j, ++k)
	    tmp.a(i, j) = (float)file_data.a_no_check(k);

    // name the fields
    EST_String t;
    // the first 'order' fields are always called c1,c2...
    // AWB bug -- the following corrupts memory
    /*    for (i = 0; i < order; i++)
	  {
	  EST_String t2;
	  t2 = EST_String("c") + itoString(i+1);
	  tmp.set_field_name(t2, i);
	  }
	  i=order;
	  */
    cout << "here \n";
    
    tmp.set_name(filename);
    tmp.set_file_type(tff_ema);
    
    fclose(fp);
    return format_ok;
}

EST_read_status EST_TrackFile::load_ema(const EST_String filename, EST_Track &tmp, float ishift, float startt)
{
  return load_ema_internal(filename, tmp, ishift, startt, FALSE);
}


EST_read_status EST_TrackFile::load_ema_swapped(const EST_String filename, EST_Track &tmp, float ishift, float startt)
{
  return load_ema_internal(filename, tmp, ishift, startt, TRUE);
}

#if 0
EST_read_status EST_TrackFile::load_NIST(const EST_String filename, EST_Track &tmp, float ishift, float startt)
{
  (void)ishift; // what does this do ?
  (void)startt;

  char header[NIST_HDR_SIZE];
  int samps,sample_width,data_length,actual_bo;
  unsigned char *file_data;
  enum EST_sample_type_t actual_sample_type;
  char *byte_order, *sample_coding;
  int n,i,j,k;
  int current_pos;
  int offset=0;

  EST_TokenStream ts;
  if (((filename == "-") ? ts.open(cin) : ts.open(filename)) != 0)
    {
      cerr << "Can't open track file " << filename << endl;
      return misc_read_error;
    }
  
  current_pos = ts.tell();
  if (ts.fread(header,NIST_HDR_SIZE,1) != 1)
    return misc_read_error;
  
  if (strncmp(header,NIST_SIG,sizeof(NIST_SIG)) != 0)
    return wrong_format;
  
  samps = nist_get_param_int(header,"sample_count",-1);
  int num_channels = nist_get_param_int(header,"channel_count",1);
  sample_width = nist_get_param_int(header,"sample_n_bytes",2);
  int sample_rate = 
    nist_get_param_int(header,"sample_rate",def_load_sample_rate);
  byte_order = nist_get_param_str(header,"sample_byte_format",
				  (EST_BIG_ENDIAN ? "10" : "01"));
  sample_coding = nist_get_param_str(header,"sample_coding","pcm");
  
  data_length = (samps - offset)*num_channels;
  file_data = walloc(unsigned char,sample_width * data_length);

  ts.seek(current_pos+NIST_HDR_SIZE+(sample_width*offset*(num_channels)));
  
  n = ts.fread(file_data,sample_width,data_length);
  
  if ((n < 1) && (n != data_length))
    {
      wfree(file_data); 
      wfree(sample_coding);
      wfree(byte_order);
      return misc_read_error;
    }
  else if ((n < data_length) && (data_length/num_channels == n))
    {
      fprintf(stderr,"TRACK read: nist header is (probably) non-standard\n");
      fprintf(stderr,"TRACK read: assuming different num_channel interpretation\n");
      data_length = n;   /* wrongly headered file */
    }
  else if (n < data_length)
    {
      fprintf(stderr,"TRACK read: short file %s\n",
	      (const char *)ts.filename());
      fprintf(stderr,"WAVE read: at %d got %d instead of %d samples\n",
	      offset,n,data_length);
	data_length = n;
    }
  
  actual_sample_type = nist_to_sample_type(sample_coding);
  actual_bo = ((strcmp(byte_order,"10") == 0) ? bo_big : bo_little);
  
  short *data;
  data = convert_raw_data(file_data,data_length,
			  actual_sample_type,actual_bo);
  
  // copy into the Track
  int num_samples = data_length/num_channels;
  tmp.resize(num_samples, num_channels);
  tmp.set_equal_space(TRUE);
  tmp.fill_time(1/(float)sample_rate);

  cerr << "shift " << 1/(float)sample_rate << endl;

  k=0;
  for (i=0; i<num_samples; i++)
    {
      for (j = 0; j < num_channels; ++j)
	tmp.a(i, j) = data[k++]; // channels are simply interleaved
      tmp.set_value(i);
    }
  for (j = 0; j < num_channels; ++j)
    tmp.set_channel_name("name", j);



  /*
  *sample_type = st_short;
  *bo = EST_NATIVE_BO;
  *word_size = 2;
  */

  //cerr << "NIST OK" << endl;
  
  return format_ok;
}

EST_write_status EST_TrackFile::save_NIST(const EST_String filename, EST_Track tr)
{
  FILE *fd;
  int i,j,k=0;
  if (filename == "-")
    fd = stdout;
  else if ((fd = fopen(filename,"wb")) == NULL)
    return write_fail;
  
  // create header
  char header[NIST_HDR_SIZE], p[1024];;
  const char *t;

  memset(header,0,1024);
  strcat(header, NIST_SIG);
  sprintf(p, "channel_count -i %d\n", tr.num_channels());
  strcat(header, p);
  sprintf(p, "sample_count -i %d\n", tr.num_frames());	
  strcat(header, p);
  int sr = (int)(rint(1/(float)tr.shift()));
  sprintf(p, "sample_rate -i %d\n", sr);	
  strcat(header, p);
  t = sample_type_to_nist(st_short);
  sprintf(p, "sample_coding -s%d %s\n", (signed)strlen(t), t);  
  strcat(header, p);

  strcat(header, NIST_END_SIG);
  /*makes it nice to read */
  strcat(header, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"); 

  // write header
  if (fwrite(&header, 1024, 1, fd) != 1)
    return misc_write_error;

  // data
  short data[tr.num_frames() * tr.num_channels()];


  for (i = 0; i < tr.num_frames(); ++i)
    // restricted to shorts at this time
    for (j = 0; j < tr.num_channels(); ++j)
      data[k++] = (short)(tr.a_no_check(i, j));
  
  // byte swapping of output not supported - only write native bo
  int bo = str_to_bo("native");
  return save_raw_data(fd,data,0,tr.num_frames(),tr.num_channels(),
		       st_short,bo);
  
  if (fd != stdout)
    fclose(fd);
  return write_ok;

}
#endif


EST_read_status EST_TrackFile::load_htk(const EST_String filename, EST_Track &tmp, float ishift, float startt)
{
    (void)ishift;
    
    // num_values is total number of fields in file
    // num_channels is number of fields in resultant track
    // order is order of LPC etc. analysis
    // e.g. if order is 12 and we have energy and delta then num_values = (12 + 1) * 2 = 26

    int i,j, order, new_frames, num_values, num_channels;

    EST_String pname;
    int swap;
    int time_included;
    
    FILE *fp;
    struct htk_header header;
    int header_sz = sizeof(header);

    // numbers A and B for decompression of generally compressed files 
    float *compressA=NULL, compressA_Buffer[REASONABLE_FRAME_SIZE];
    float *compressB=NULL, compressB_Buffer[REASONABLE_FRAME_SIZE];
    bool fileIsCompressed=false;

    unsigned short samp_type, base_samp_type;
    
    if ((fp = fopen(filename, "rb")) == NULL){
      cerr << "EST_Track load: couldn't open EST_Track input file" << endl;
      return misc_read_error;
    }
    
    // try and read the header
    if (fread(&header, header_sz, 1, fp) != 1){
      fclose(fp);
      return wrong_format;
    }
    
    swap = htk_swapped_header(&header); // this is regrettable

    if( swap<0 ){
      fclose(fp);
      return read_format_error;
    }
    
    samp_type = header.samp_type;
    base_samp_type = samp_type & HTK_MASK;
    
    time_included = (samp_type & HTK_EST_PS) != 0;
    
    switch(base_samp_type){
    case HTK_WAVE:
      cerr << "Can't read HTK WAVEFORM format file into track" << endl;
      return misc_read_error;
      break;
      
    case HTK_LPC:
      pname = "ct_lpc";
      break;

    case HTK_LPCREFC:
    case HTK_IREFC:
      EST_warning( "reading HTK_IREFC and HTK_LPREC parameter types is unsupported" );
      fclose( fp );
      return read_format_error;
      break;
       
    case HTK_LPCCEP:
      pname = "ct_cepstrum";
      break;
      
    case HTK_LPDELCEP:
      // equivalent to HTK_LPCCEP + DELTA
      base_samp_type = HTK_LPCCEP;
      samp_type = HTK_LPCCEP | HTK_DELTA; // set delta bit 
      pname = "ct_cepstrum";
      break;
      
    case HTK_MFCC:
      pname = "ct_other";
      break;
      
    case HTK_FBANK:
    case HTK_USER:
      pname = "ct_other";
      break;
      
    case HTK_DISCRETE:
      cerr << "Can't read HTK DISCRETE format file into track" << endl;
      return misc_read_error;
      break;
      
    case HTK_MELSPEC:
      pname = "ct_other";
      break;
      
    default:
      fclose(fp);
      return wrong_format;
      break;
    }
    
    // if we get this far we have decided this is a HTK format file
    
    // handle compressed/uncompressed files differently
    if( header.samp_type & HTK_COMP ){

      fileIsCompressed = true;

      num_channels = num_values = header.samp_size / sizeof(short int);

      // get compression numbers A and B
      if (num_channels > REASONABLE_FRAME_SIZE){
	compressA = new float[num_values];
	compressB = new float[num_values];
      }
      else{
	compressA = compressA_Buffer;
	compressB = compressB_Buffer;
      }

      if( (fread( compressA, sizeof(float), num_values, fp )) != static_cast<size_t>(num_values) ){
	fclose( fp );
	return read_format_error;
      }
      
      if( (fread( compressB, sizeof(float), num_values, fp )) != static_cast<size_t>(num_values) ){
	fclose( fp );
	return read_format_error;
      }

      if (swap){
	swap_bytes_float( compressA, num_values );
	swap_bytes_float( compressB, num_values );
      }	

      // subtract extra frames to account for the two vectors of floats
      // used for decompression.
      new_frames = header.num_samps - (2*(sizeof(float)-sizeof(short int)));
    }
    else{
      num_channels = num_values = header.samp_size / sizeof(float);
      new_frames = header.num_samps;
    }
    
    if (num_values > UNREASONABLE_FRAME_SIZE){
      fclose(fp);
      return read_format_error;
    }
    
    if (time_included)
      num_channels -= 1;
    
    float shift = ((float)header.samp_period/ (float)HTK_UNITS_PER_SECOND);
    
    tmp.resize(new_frames, num_channels);

    if ((startt > 0) && (startt < NEARLY_ZERO ))
	EST_warning( "setting htk file start to %f", startt );

    tmp.fill_time(shift, startt);

    tmp.set_equal_space(!time_included);
    
    // check length of file is as expected from header info
    long dataBeginPosition = ftell(fp);
    if( dataBeginPosition == -1 ){
      fclose(fp);
      return wrong_format;
    }
    
    if (fseek(fp,0,SEEK_END)){
      fclose(fp);
      return wrong_format;
    }
    
    long file_length;
    if ((file_length = ftell(fp)) == -1){
      fclose(fp);
      return wrong_format;
    }
    
    long expected_vals;
    if( fileIsCompressed ){
      expected_vals = (file_length-dataBeginPosition) / sizeof(short int);      
      
      if( header.samp_type & HTK_CRC )
	expected_vals -= 1; // just ignore the appended cyclic redundancy checksum
    }
    else
      expected_vals = (file_length-dataBeginPosition) / sizeof(float);      
    
    /*
      printf( "%d %d %d %d %d %d\n",
      expected_vals, file_length, dataBeginPosition, sizeof(float), num_values, new_frames );
    */
    
    if( expected_vals != (num_values * new_frames) ){
      // it probably isn't HTK format after all
      fclose(fp);
      return wrong_format;
    }
    
    // work out the order of the analysis
    // Reorg -- surely you can't increase order
    order = num_channels;
    if( samp_type & HTK_NO_E ) 
      order++;
    
    if( samp_type & HTK_AC )
      order /= 3;
    else if( samp_type & HTK_DELTA )
      order /= 2;
    
    if( samp_type & HTK_ENERGY )
      order--;
    
    // go to start of data
    if( fseek(fp, dataBeginPosition, SEEK_SET) == -1 ){
      cerr << "Couldn't position htk file at start of data" << endl;
      fclose(fp);
      return misc_read_error;
    }

    if( fileIsCompressed ){
      short int *frame, frame_buffer[REASONABLE_FRAME_SIZE];
      if( num_values > REASONABLE_FRAME_SIZE )
	frame = new short int[num_values];
      else
	frame = frame_buffer;

      int first_channel = time_included?1:0;
      
      for( i=0; i<new_frames; i++ ){
	if( fread( frame, sizeof(short int), num_values, fp ) != (size_t) num_values ){
	  cerr << "Could not read data from htk track file" << endl;
	  fclose(fp);
	  
	  if( frame != frame_buffer )
	    delete [] frame;
	  if( compressA != compressA_Buffer )
	    delete [] compressA;
	  if( compressB != compressB_Buffer )
	    delete [] compressB;

	  return misc_read_error;
	}

	if( swap )
	  swap_bytes_short( frame, num_values );
	
	if( time_included )
	  tmp.t(i) = ((float)frame[0]+compressB[0])/compressA[0];
	
	for( j=0; j<num_channels; ++j ){
	  int index = j+first_channel;
	  tmp.a(i,j) = ((float)frame[index]+compressB[index])/compressA[index];
	}

	tmp.set_value(i);
      }

      if( frame != frame_buffer )
	delete [] frame;
      if( compressA != compressA_Buffer )
	delete [] compressA;
      if( compressB != compressB_Buffer )
	delete [] compressB;
    }
    else{
      float *frame, frame_buffer[REASONABLE_FRAME_SIZE];
    
      if (num_values > REASONABLE_FRAME_SIZE)
	frame = new float[num_values];
      else
	frame = frame_buffer;
    
      int first_channel = time_included?1:0;
      for( i=0; i<new_frames; i++ ){
	if( fread( frame, sizeof(float), num_values, fp ) != (size_t) num_values ){
	  cerr << "Could not read data from htk track file" << endl;
	  fclose(fp);
	  if (frame != frame_buffer)
	    delete [] frame;
	  return misc_read_error;
	}
	if( swap )
	  swap_bytes_float( frame, num_values );
	
	if( time_included )
	  tmp.t(i) = frame[0];
      
	for( j=0; j<num_channels; ++j )
	  tmp.a(i, j) = frame[j+first_channel];

	tmp.set_value(i);
      }
    
      if( frame != frame_buffer )
	delete [] frame;
    }
    
    // name the fields
    EST_String t;
    // the first 'order' fields are always called c1,c2...
    // AWB bug -- the following corrupts memory
    for (i=0;i<order;i++)
    {
	EST_String t2;
	t2 = EST_String("c") + itoString(i+1);
	tmp.set_channel_name(t2, i);
    }
    i=order;
    
    // energy present and not suppressed
    if ( (samp_type & HTK_ENERGY) && !(samp_type & HTK_NO_E) )
      tmp.set_channel_name("E", i++);
    
    // delta coeffs ?
    if (samp_type & HTK_DELTA){
      for (j = 0; j < order; j++){
	t = EST_String("c") + itoString(j+1) + "_d";
	tmp.set_channel_name(t, i++);
      }
      
      // energy ?
      if (samp_type & HTK_ENERGY)
	tmp.set_channel_name("E_d", i++);
    }
    
    // 'acceleration'  coeffs ?
    if (samp_type & HTK_AC){
      for(j=0;j<order;j++){
	t = EST_String("ac")+ itoString(j+1)+ "_d_d";
	tmp.set_channel_name(t, i++);
      }
      // energy ?
      if (samp_type & HTK_ENERGY)
	tmp.set_channel_name("E_d_d", i++);
    }
    
    // sanity check
    if (i != num_channels){
      cerr << "Something went horribly wrong - wanted " << num_values
	   << " channels in track but got " << i << endl;
      fclose(fp);
      return wrong_format;
    }
    tmp.f_set("contour_type",pname);
    tmp.set_name(filename);
    tmp.set_file_type(tff_htk);
    fclose(fp);
    return format_ok;
}

/************************************************************************/
/*                                                                      */
/* Convert single f0 channel tracks into arbitrarily chosen esps FEA    */
/* subtype, reputedly to make waves happy. This is evil beyond all      */
/* understanding.                                                       */
/*                                                                      */
/************************************************************************/

// format of the desired track.
static struct EST_TrackMap::ChannelMappingElement espsf0_mapping[] =
{
{ channel_f0, 0 },
{ channel_voiced,  1 },
{ channel_power, 2},
{ channel_peak, 3},
{ channel_unknown, 0}	
};
static EST_TrackMap ESPSF0TrackMap(espsf0_mapping);

// It seems that the vital thing is to call the track "F0", and  so
// we only need 1 channel instead of the normal 5. This saves  *lots* of
// space. For the time being we use 2 channels as the prob_voicnug filed is
// used by our input routine.

int track_to_espsf0(EST_Track &track, EST_Track &f0_track)
{
    f0_track.resize(track.num_frames(), 2);
    
    f0_track.assign_map(ESPSF0TrackMap);
    
    // k1 is ratio of the first two cross-correlation values
    //    f0_track.set_channel_name("k1", 4);
    
    // copy data. Remaining channels zeroed by resize. This is of course stupid
    // as if k1 iz zero mathematics is in deep problems.
    for (int i = 0; i < track.num_frames(); ++i)
    {
	f0_track.a(i, channel_voiced) = track.track_break(i) ? 0.1 : 1.2;
	f0_track.a(i, channel_f0) = track.track_break(i) ? 0.0: track.a(i,0);
    }
    
    f0_track.set_file_type(tff_esps);
    f0_track.fill_time(track.shift());
    track.set_name(track.name());
    
    /*    f0_track.resize(track.num_frames(), 5);
	  
	  f0_track.assign_map(ESPSF0TrackMap);
	  
	  // k1 is ratio of the first two cross-correlation values
	  f0_track.set_channel_name("k1", 4);
	  
	  // copy data. Remaining channels zeroed by resize. This is of course stupid
	  // as if k1 iz zero mathematics is in deep problems.
	  for (int i = 0; i < track.num_frames(); ++i)
	  {
	  f0_track.a(i, channel_voiced) = track.track_break(i) ? 0.1 : 1.2;
	  f0_track.a(i, channel_f0) = track.a(i,0);
	  }
	  
	  f0_track.set_file_type("esps");
	  f0_track.fill_time(track.shift());
	  track.set_name(track.name());
	  */
    
    return 0;
}

int espsf0_to_track(EST_Track &fz)
{
    int f, p, i;
    f = p = -1;
    
    // check to see if prob of voicing channel exists
    for (i = 0; i < fz.num_channels(); ++i)
    {
	if (fz.channel_name(i) == "prob_voice")
	    p = i;
    }
    for (i = 0; i < fz.num_channels(); ++i)
    {
	if (fz.channel_name(i) == "F0")
	    f = i;
    }
    
    for (i = 0; i < fz.num_frames(); ++i)
    {
	if (p == -1)		// if f0 val is < 1 make this a break
	{
	    if (fz.a(i, f) < 1.0)
		fz.set_break(i);
	    else
		fz.set_value(i);
	}
	else			// use prob voicing
	{
	    if (fz.a(i, p) < 0.5)
	    {
		fz.a(i, f) = 0.0;
		fz.set_break(i);
	    }
	    else
		fz.set_value(i);
	}
    }
    
    return 0;
}

int track_to_htk_lpc(EST_Track &track, EST_Track &lpc)
{
    int type = HTK_LPC;
    int ncoefs, nchannels;
    
    if (track.has_channel(channel_lpc_N))
	ncoefs = track.channel_position(channel_lpc_N) - track.channel_position(channel_lpc_0)+1;
    else
	ncoefs = track.num_channels()-track.channel_position(channel_lpc_0);
    
    nchannels = ncoefs;
    
    if (track.has_channel(channel_power))
    {
	nchannels++;
	type |= HTK_ENERGY;
    }
    
    lpc.resize(track.num_frames(), nchannels);
    lpc.set_equal_space(track.equal_space());
    lpc.set_single_break(track.single_break());
    lpc.set_single_break(track.single_break());
    
    for(int i = 0; i< track.num_frames(); i++)
	for (int c = 0; c < ncoefs; c++)
	{
	    lpc.a(i, c) = track.a(i, channel_lpc_0, c);
	    lpc.t(i) = track.t(i);
	}
    
    
    if (track.has_channel(channel_power))
    {
	for(int ii = 0; ii< track.num_frames(); ii++)
	    lpc.a(ii, ncoefs) = track.a(ii, channel_power);
    }
    
    return type;
    
}

EST_write_status save_ind_TrackList(EST_TrackList &tlist, EST_String &otype)
{
    for (EST_Litem *p = tlist.head(); p ; p = p->next())
	tlist(p).save(tlist(p).name(), otype);
    
    return write_ok;
}    

EST_read_status read_TrackList(EST_TrackList &tlist, EST_StrList &files, 
			       EST_Option &al)
{
    EST_Track s;
    EST_Litem *p, *plp;
    
    for (p = files.head(); p; p = p->next())
    {
	tlist.append(s);
	plp = tlist.tail();
	if (read_track(tlist(plp), files(p), al) != format_ok)
	    exit (-1);

	tlist(plp).set_name(files(p));
    }
    
    return format_ok;
}

int read_track(EST_Track &tr, const EST_String &in_file, EST_Option &al)
{
    
    float ishift = 0;
    float startt = 0.0;
    
    if( al.present("-startt") )
      startt = al.fval( "-startt" );

    if (al.present("ishift"))
	ishift = al.fval("ishift");
    else if (al.present("-s"))
	ishift = al.fval("-s");
    else if (al.present("time_channel"))
	ishift = 1.0;		// doesn't matter, will be reset by track
    
    if (al.present("-itype"))
    {
	if (tr.load(in_file, al.val("-itype", 0), ishift, startt) != format_ok)
	    return -1;
    }
    else
    {
	if (tr.load(in_file, ishift, startt ) != format_ok)
	    return -1;
    }
    
//    tr.create_map();
    
    // cout << "f0 "<< tr.has_channel(channel_f0) << ".\n";
//    if (al.present("time_channel") && tr.has_channel(al.sval("time_channel")))
//    {
//	cout << " time from channel " << al.sval("time_channel") << "\n";
//	channel_to_time(tr, al.sval("time_channel"), al.fval("time_scale"));
//    }
    
    
    //    cout << tr;
    return 0;
}


EST_String EST_TrackFile::options_short(void)
{
    EST_String s("");
    
    for(int n=0; n< EST_TrackFile::map.n() ; n++)
    {
	const char *nm = EST_TrackFile::map.name(EST_TrackFile::map.token(n));
	
	if (s != "")
	    s += ", ";
	
	s += nm;
	
    }
    return s;
}

EST_String EST_TrackFile::options_supported(void)
{
    EST_String s("AvailablE track file formats:\n");
    
    for(int n=0; n< EST_TrackFile::map.n() ; n++)
    {
	const char *nm = EST_TrackFile::map.name(EST_TrackFile::map.token(n));
	const char *d = EST_TrackFile::map.info(EST_TrackFile::map.token(n)).description;
	
	s += EST_String::cat("        ", nm, EST_String(" ")*(13-strlen(nm)), d, "\n");
    }
    return s;
}

// note the order here defines the order in which loads are tried.
static EST_TValuedEnumDefinition<EST_TrackFileType, const char *, EST_TrackFile::Info> trackfile_names[] =
{
{ tff_none,             { "none" },
{FALSE, NULL, NULL,
 "unknown track file type"}},
{tff_esps,		{ "esps" }, 
{TRUE, EST_TrackFile::load_esps, EST_TrackFile::save_esps,
 "entropic sps file"}},
{tff_est_ascii,		{ "est", "est_ascii" }, 
{TRUE, EST_TrackFile::load_est, EST_TrackFile::save_est_ascii,
 "Edinburgh Speech Tools track file"}},
{tff_est_binary,		{ "est_binary" }, 
{TRUE, EST_TrackFile::load_est, EST_TrackFile::save_est_binary,
 "Edinburgh Speech Tools track file"}}
,
{tff_htk,		{ "htk" }, 
{TRUE, EST_TrackFile::load_htk, EST_TrackFile::save_htk,
 "htk file"}},
//{tff_NIST,	{ "NIST" }, 
//{TRUE, EST_TrackFile::load_NIST, EST_TrackFile::save_NIST,
// "NIST"}},
{tff_htk_fbank,	{ "htk_fbank" }, 
{FALSE, EST_TrackFile::load_htk, EST_TrackFile::save_htk_fbank,
 "htk file (as FBANK)"}},
{tff_htk_mfcc,	{ "htk_mfcc" }, 
{FALSE, EST_TrackFile::load_htk, EST_TrackFile::save_htk_mfcc,
 "htk file (as MFCC)"}},
{tff_htk_mfcc_e,	{ "htk_mfcc_e" }, 
{FALSE, EST_TrackFile::load_htk, EST_TrackFile::save_htk_mfcc_e,
 "htk file (as MFCC_E)"}},
{tff_htk_user,	{ "htk_user" }, 
{FALSE, EST_TrackFile::load_htk, EST_TrackFile::save_htk_user,
 "htk file (as USER)"}},
{tff_htk_discrete,	{ "htk_discrete" }, 
{FALSE, EST_TrackFile::load_htk, EST_TrackFile::save_htk_discrete,
 "htk file (as DISCRETE)"}},
{tff_ssff,		{"ssff"}, 
{TRUE, EST_TrackFile::load_ssff, EST_TrackFile::save_ssff,
 "Macquarie University's Simple Signal File Format"}},
{tff_xmg,		{ "xmg" }, 
{TRUE, EST_TrackFile::load_xmg, EST_TrackFile::save_xmg,
 "xmg file viewer"}},
{tff_xgraph,		{ "xgraph" }, 
{FALSE, EST_TrackFile::load_xgraph, EST_TrackFile::save_xgraph,
 "xgraph display program format"}},
{tff_ema,		{ "ema" }, 
{FALSE, EST_TrackFile::load_ema, NULL,
 "ema"}},
{tff_ema_swapped,	{ "ema_swapped" }, 
{FALSE, EST_TrackFile::load_ema_swapped, NULL,
 "ema, swapped"}},
{tff_ascii,		{ "ascii" }, 
{TRUE, EST_TrackFile::load_ascii, EST_TrackFile::save_ascii,
 "ascii decimal numbers"}},
{ tff_none,  {"none"},  {FALSE, NULL, NULL, "unknown track file type"} }
};

EST_TNamedEnumI<EST_TrackFileType, EST_TrackFile::Info> EST_TrackFile::map(trackfile_names);

static EST_TValuedEnumDefinition<EST_TrackFileType, const char *, 
EST_TrackFile::TS_Info> track_ts_names[] =
{
{ tff_none,		{ "none" },				
{FALSE, NULL, NULL,
 "unknown track file type"}},
 
{tff_est_ascii,		{"est"}, 
{TRUE, EST_TrackFile::load_est_ts, EST_TrackFile::save_est_ts,
 "Edinburgh Speech Tools track file"}},

{tff_est_binary,	{"est_binary"}, 
{TRUE, EST_TrackFile::load_est_ts, EST_TrackFile::save_est_binary_ts,
 "Edinburgh Speech Tools track file"}},

{tff_ssff,		{"ssff"}, 
{TRUE, EST_TrackFile::load_ssff_ts, EST_TrackFile::save_ssff_ts,
 "Macquarie University's Simple Signal File Format"}},

{ tff_none,		{ "none" },
{FALSE, NULL, NULL,
 "unknown track file type"}}
};

EST_TNamedEnumI<EST_TrackFileType, EST_TrackFile::TS_Info> 
EST_TrackFile::ts_map(track_ts_names);


#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TNamedEnum.cc"
template class EST_TNamedEnumI<EST_TrackFileType, EST_TrackFile::Info>;
template class EST_TValuedEnumI<EST_TrackFileType, 
const char *, EST_TrackFile::Info>;
template class EST_TNamedEnumI<EST_TrackFileType, EST_TrackFile::TS_Info>;
template class EST_TValuedEnumI<EST_TrackFileType, 
const char *, EST_TrackFile::TS_Info>;

#endif

