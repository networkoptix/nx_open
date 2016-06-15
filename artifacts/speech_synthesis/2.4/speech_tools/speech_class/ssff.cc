/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1999                            */
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
/*                 Author :  Alan W Black                                */
/*                 Date   :  November 1999                               */
/*-----------------------------------------------------------------------*/
/* Version of code for reading and writing MacQuarie's SSFF format as    */
/* used in emulabel                                                      */
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

EST_read_status EST_TrackFile::load_ssff(const EST_String filename, 
					EST_Track &tr, float ishift, float startt)
{
    EST_TokenStream ts;
    
    if (((filename == "-") ? ts.open(cin) : ts.open(filename)) != 0)
    {
	cerr << "Can't open track file " << filename << endl;
	return misc_read_error;
    }
    tr.set_name(filename);
    return load_ssff_ts(ts, tr, ishift, startt);
}

EST_read_status EST_TrackFile::load_ssff_ts(EST_TokenStream &ts, EST_Track &tr, float ishift, float startt)
{
    (void)ishift;
    (void)startt;
    int num_frames, num_channels;
    int swap = FALSE;
    int i,j,pos,end;
    float Start_Time, Record_Freq;
    EST_Features channels;
    EST_String c, name, type, size, cname;
    FILE *fp;
    double dbuff[2];
    short  sbuff[2];

    num_frames = num_channels = 0;
    Start_Time = Record_Freq = 0;

    if (ts.get() != "SSFF")
	return wrong_format;
    
    if ((ts.get() != "--") ||
	(ts.get() != "(c)") ||
	(ts.get() != "SHLRC"))
    {
	cerr << "ssff load track \"" << ts.filename() << "\": bad header"
	     << endl;
	return misc_read_error;
    }
    
    while (ts.peek() != "-----------------")
    {
	c = (EST_String)ts.get();
	if (c == "Comment")
	    ts.get_upto_eoln();
	else if (c == "Start_Time")
	{
	    Start_Time = atof(ts.get().string());
	    tr.f_set("Start_Time",Start_Time);
	}
	else if (c == "Record_Freq")
	{
	    Record_Freq = atof(ts.get().string());
	    tr.f_set("Record_Freq",Record_Freq);
	}
	else if (c == "Machine")
	{
	    if (ts.get() == "SPARC")
	    {
		if (EST_NATIVE_BO != bo_big)
		    swap = TRUE;
	    }
	    else if (EST_NATIVE_BO == bo_big)
		swap = TRUE;
	}
	else if (c == "Column")
	{
	    name = (EST_String)ts.get();
	    type = (EST_String)ts.get();
	    size = (EST_String)ts.get();
	    cname = EST_String("Channel_")+itoString(num_channels);
	    channels.set(cname+".name",name);
	    channels.set(cname+".type",type);
	    channels.set(cname+".size",atoi(size));
	    num_channels++;
	}
	else if ((c == "window_type") ||
		 (c == "window_duration") ||
		 (c == "lpc_order") ||
		 (c == "lpc_type") ||
		 (c == "end_time") ||
		 (c == "preemphasis") ||
		 (c == "frame_duration"))
	{
		type = (EST_String)ts.get();
		if (type == "SHORT")
		    tr.f_set(c,atoi(ts.get().string()));
		else if (type == "DOUBLE")
		    tr.f_set(c,(float)atof(ts.get().string()));
		else
		    tr.f_set(c,ts.get().string());
	}
	else if (ts.eof())
	{
	    cerr << "ssff load track \"" << ts.filename() << 
		"\": bad header unexpected eof" << endl;
	    return misc_read_error;
	}
        //	else 
        //	{
        //	    cerr << "ssff load track \"" << ts.filename() << 
        //		"\": unknown header value \"" << c << "\"" << endl;
        //	}
    }
    ts.get();  // skip over end of header line

    // There's no num_records field in the header so have to use file's 
    // length to calculate it
    fp = ts.filedescriptor();
    pos = ftell(fp);
    fseek(fp,0,SEEK_END);
    end = ftell(fp);
    fseek(fp,pos,SEEK_SET);
    num_frames = (end - pos)/(num_channels*sizeof(double));
    
    // Finished reading header
    tr.resize(num_frames,num_channels);
    tr.fill_time(1.0/Record_Freq);
    tr.set_equal_space(true);
    
    for (i=0; i<num_channels; i++)
	tr.set_channel_name(channels.S(EST_String("Channel_")+
				       itoString(i)+".name"),i);
    
    for (i=0; i < num_frames; i++)
	for (j=0; j<num_channels; j++)
	{
	    type = channels.S(EST_String("Channel_")+ itoString(j)+".type");
	    if (type == "DOUBLE")
	    {
		ts.fread(dbuff,sizeof(double),1);
		if (swap)
		    swap_bytes_double(dbuff,1);
		tr(i,j) = *dbuff;
	    }
	    else if (type == "SHORT")
	    {
		ts.fread(sbuff,sizeof(short),1);
		if (swap)
		    swap_bytes_short(sbuff,1);
		tr(i,j) = (float)(*sbuff);
	    }
	    else
	    {
		cerr << "ssff load track \"" << ts.filename() << 
  	         "\": unknown channel type value \"" << type << "\"" << endl;
		return misc_read_error;
	    }
	}

    return format_ok;
}

EST_write_status EST_TrackFile::save_ssff(const EST_String filename, EST_Track tr)
{
    FILE *fd;
    EST_write_status r;

    if (filename == "-")
	fd = stdout;
    else if ((fd = fopen(filename,"wb")) == NULL)
	return write_fail;

    r = save_ssff_ts(fd,tr);
    
    if (fd != stdout)
	fclose(fd);
    return r;

}

EST_write_status EST_TrackFile::save_ssff_ts(FILE *fp, EST_Track tr)
{
    int i,j;
    int need_prob_voice = 0;
    
    if (tr.equal_space() != 1)
    {
	cerr << "ssf save track: can't save variable spaced track as SSFF"
	     << endl;
	return write_error;
    }

    fprintf(fp,"SSFF -- (c) SHLRC\n");
    if (EST_NATIVE_BO == bo_big)
	fprintf(fp,"Machine SPARC\n");
    else 
	fprintf(fp,"Machine IBM-PC\n");
    if (tr.f_present("Start_Time"))
	fprintf(fp,"Start_Time %g\n",(double)tr.f_F("Start_Time"));
    else
	fprintf(fp,"Start_Time 0.000000\n");
    // If there are less than two sample points I can't get this value
    if (tr.f_present("Record_Freq"))
	fprintf(fp,"Record_Freq %g\n",(double)tr.f_F("Record_Freq"));
    else if (tr.num_frames() < 2)
	fprintf(fp,"Record_Freq %d\n", 100);
    else
	fprintf(fp,"Record_Freq %g\n", 1/(tr.t(1)-tr.t(0)));
    
    for (i = 0; i < tr.num_channels(); ++i)
	fprintf(fp, "Column %s DOUBLE 1\n",(const char *)(tr.channel_name(i)));
    // For EMULABEL to read this is needs prob_voice too
    if ((tr.num_channels() == 1) &&
	(tr.channel_name(0) == "F0"))
    {
	need_prob_voice = 1;
	fprintf(fp, "Column prob_voice DOUBLE 1\n");
    }
    EST_Featured::FeatEntries p;

    for (p.begin(tr); p; ++p)
    {
	if ((p->k == "Start_Time") ||
	    (p->k == "Record_Freq"))
	    continue;
	else
	    fprintf(fp, "%s DOUBLE %s\n", (const char *)p->k,
		    (const char *) p->v.String());
    }
    fprintf(fp,"-----------------\n");
    for (i=0; i< tr.num_frames(); i++)
    {
	double prob_voice;
	double dd;
	for (j=0; j< tr.num_channels(); j++)
	{
	    dd = tr(i,j);
	    fwrite(&dd,sizeof(double),1,fp);
	}
	if (need_prob_voice)
	{
	    if (tr(i,0) == 0)
		prob_voice = 0;
	    else
		prob_voice = 1;
	    fwrite(&prob_voice,sizeof(double),1,fp);
	}
    }

    return write_ok;
}
