/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                    Copyright (c) 1994,1995,1996                       */
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
/*                      Author :  Paul Taylor                            */
/*                      Date   :  June 1994                              */
/*-----------------------------------------------------------------------*/
/*                  EST_Track file manipulation program                  */   
/*                                                                       */
/*=======================================================================*/

#include "EST.h"
#include "EST_cmd_line_options.h"

#define DEFAULT_TIME_SCALE 0.001

int StrListtoIList(EST_StrList &s, EST_IList &il);
void extract_channel(EST_Track &orig, EST_Track &nt, EST_IList &ch_list);

EST_write_status save_snns_pat(const EST_String filename, 
			       EST_TrackList &inpat, EST_TrackList &outpat);

EST_read_status read_TrackList(EST_TrackList &tlist, EST_StrList &files, 
			       EST_Option &al);

void extract(EST_Track &tr, EST_Option &al);
/** @name <command>ch_track</command> <emphasis>Track file manipulation</emphasis>
  * @id ch-track-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**
ch_track is used to manipulate the format of a track
file. Operations include:

<itemizedlist>
<listitem><para>file format conversion</para></listitem>
<listitem><para>smoothing</para></listitem>
<listitem><para>changing the frame spacing of a track (resampling)</para></listitem>
<listitem><para>producing differentiated and delta tracks</para></listitem>
<listitem><para>Using a threshold to convert a track file to a label file</para></listitem>

<listitem><para>making multiple input files into a single multi-channel output file</para></listitem>
<listitem><para>extracting a single channel from a multi-channel track</para></listitem>
<listitem><para>extracting a time-delimited portion of the waveform</para></listitem>
</itemizedlist>

 */

//@}

/**@name Options
  */
//@{

//@options

//@}


int main(int argc, char *argv[])
{
    EST_String in_file("-"), out_file("-");
    EST_Option al, settings;
    EST_String fname, ftmp;
    EST_StrList files;
    EST_Track tr;
    EST_TrackList trlist;
    EST_Litem *p;

    parse_command_line(
	argc, argv, 
	EST_String("[input file] -o [output file] [options]\n")+
	"Summary: change/copy track files\n"
	"use \"-\" to make input and output files stdin/out\n"
	"-h               Options help\n"+
	options_track_input()+ "\n"+
	options_track_output()+	"\n"
	"-info  Print information about file and header. \n"
	"    This option gives useful information such as file \n"
	"    length, file type, channel names. No output is produced\n\n"
	"-track_names <string> \n"
	"    File containing new names for output channels\n\n"
	"-diff Differentiate contour. This performs simple \n"
	"    numerical differentiation on the contour by \n"
	"    subtracting the amplitude of the current frame \n"
        "    from the amplitude of the next. Although quick, \n"
	"    this technique is crude and not recommende as the \n"
	"    estimation of the derivate is done on only one point\n\n"
	"-delta <int> Make delta coefficients (better form of differentiate).\n"
	"    The argument to this option is the regression length of \n"
	"    of the delta calculation and can be between 2 and 4 \n\n"
	"-sm <float> Length of smoothing window in seconds. Various types of \n"
	"    smoothing are available for tracks. This options specifies \n"
	"    length of the smooting window which effects the degree of \n"
	"    smoothing, i.e. a longer value means more smoothing \n\n"
	"-smtype <string>  Smooth type, median or mean\n"
	"-style <string>  Convert track to other form.  Currently only one form \n"
	"   \"label\" is supported. This uses a specified cut off to \n"
	"    make a label file, with two labels, one for above the \n"
	"    cut off (-pos) and one for below (-neg)\n\n"
	"-t <float> threshold for track to label conversion \n"
	"-neg <string> Name of negative label in track to label conversion \n"
	"-pos <string> Name of positive label in track to label conversion \n"
	"-pc <string>  Combine given tracks in parallel.  If option \n"
	"     is longest, pad shorter tracks to longest, else if \n"
	"     first pad/cut to match first input track \n" +
	options_track_filetypes_long(),
	files, al);

/*redundant options
	"-time_channel <string>\n"+
	"		 Which track in track file holds pitchmark times\n"+
	"-time_scale <float>    \n"+
	"		 Scale of pitchmarks (default 0.001 = milliseconds)\n"+
*/


    override_lib_ops(settings, al);
    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";

    EST_TokenStream ts;
    
//    ts.open(files.first());
//    tr.load(ts);
//    cout << tr;

    if (read_TrackList(trlist, files, al) != read_ok)
	exit(0);

    if (files.length() == 0)
    {
	cerr << argv[0] << ": no input files specified\n";
	exit(-1);
    }

    if (al.present("-info"))
    {
	for (p = trlist.head(); p; p = p->next())
	    track_info(trlist(p));
	exit(0);
    }

    if (al.present("-pc"))       // parallelize them
	ParallelTracks(tr, trlist, al.val("-pc"));

    else if (al.val("-otype", 0) == "snns")
    {   // sometime this will generalise for multiple input files
	EST_TrackList inpat, outpat;
	inpat.append(trlist.nth(0));
	outpat.append(trlist.nth(1));
	save_snns_pat(out_file, inpat, outpat);
	exit(0);
    }
    else                         // concatenate them
    {
	tr.resize(0, tr.num_channels());
	// Reorg -- fix += to resize to largest num_channels (with warning)
	for (p = trlist.head(); p; p = p->next())
	    tr += trlist(p);
    }

    if (al.present("-S"))
	tr.sample(al.fval("-S"));
    if (al.present("-sm"))
    {
	track_smooth(tr, al.fval("-sm"),al.val("-smtype"));
    }

    if (al.present("-diff") && al.present("-delta"))
    {
	cerr << "Using -diff and -delta together makes no sense !\n";
	exit(-1);
    }
    if (al.present("-diff"))
    {
	tr = differentiate(tr);
    }
    if (al.present("-delta"))
    {
	EST_Track ntr = tr; // to copy size !;
	delta(tr,ntr,al.ival("-delta"));
	tr = ntr;
    }

    if (al.present("-c"))
    {
	EST_StrList s;
	EST_Track ntr;
	EST_IList il;
	StringtoStrList(al.val("-c"), s, " ,"); // separator can be space or comma
	StrListtoIList(s, il);
	extract_channel(tr, ntr, il);
	tr = ntr;
    }

    if (al.present("-start") || al.present("-end") 
	|| al.present("-to") || al.present("-from"))
	extract(tr, al);

//    tr.assign_map(&LPCTrackMap);
//    tr.set_space_type("VARI");


    // optionally rename output tracks before saving

    if (al.present("-track_names"))
    {
	EST_StrList new_names;
	if(load_StrList(al.val("-track_names"),new_names) != format_ok)
	{
	    cerr << "Failed to load new track names file." << endl;
	    exit(-1);
	}
	/*
	if (tr.num_channels() != new_names.length())
	{
	    cerr << "Number of names in output track names file (";
	    cerr << new_names.length() << ") " << endl;
	    cerr << " does not match number of output channels (";
	    cerr << tr.num_channels() << ")" << endl;
	    exit(-1);
	}

	EST_Litem *np;
	int ni;
	for (np = new_names.head(),ni=0; np; np = np->next(),ni++)
	    tr.set_channel_name(new_names(np),ni);
	*/
	tr.resize(EST_CURRENT, new_names);
    }

    // track_info(tr);

/*    tr.resize(EST_CURRENT, 10);

    cout << "new\n";
    track_info(tr);

    EST_StrList x;
    x.append("a");
    x.append("c");
    x.append("d");



    cout << "new\n";
    track_info(tr);
*/


    // Write out file in appropriate format
  
    if (al.val("-style",0) == "label")
    {
	EST_Relation lab;
	if (al.present("-t"))
	    track_to_label(tr, lab, al.fval("-t"));
	else
	    track_to_label(tr, lab);
	if (al.present("-pos"))
	    change_label(lab, "pos", al.val("-pos"));
	if (al.present("-neg"))
	    change_label(lab, "neg", al.val("-neg"));
	if (lab.save(out_file) != write_ok)
	    exit(-1);
    }
/*    else if (al.val("-style",0) == "pm")
    {
	EST_Relation lab;
	
	if (!al.present("-f"))
	{
	    cerr << "must specify sample rate (with -f) for pm style\n";
	    exit(-1);
	}
	int sample_rate = al.ival("-f", 0);

	track_to_pm(tr, sample_rate, lab);

	if (lab.save(out_file) != write_ok)
	    exit(-1);
    }
*/
    else
    {
	if (tr.save(out_file, al.val("-otype")) != write_ok)
	    exit(-1);
    }
    
    return 0;
}

void override_lib_ops(EST_Option &a_list, EST_Option &al)
{
    a_list.override_val("ishift", al.val("-s", 0));
    a_list.override_val("color", al.val("-color", 0));
    a_list.override_val("in_track_file_type", al.val("-itype", 0));
    a_list.override_val("out_track_file_type", al.val("-otype", 0));
    a_list.override_val("tr_to_label_thresh", al.val("-t", 0));
    a_list.override_fval("time_scale", DEFAULT_TIME_SCALE);
    
    if (al.val("-style", 0) == "label")
	a_list.override_val("lab_file_type", al.val("-otype", 0));
    if (al.present("-time_scale"))
	a_list.override_fval("time_scale", al.fval("-time_scale", 1));
    if (al.present("-time_channel"))
	a_list.override_val("time_channel", al.sval("-time_channel", 1));
}


/** @name Making multiple tracks into a single track

If multiple input files are specified, by default they are concatenated into 
the output file.
<para>
<screen>
$ ch_track kdt_010.tr kdt_011.tr kdt_012.tr kdt_013.tr -o out.tr
</screen>
</para>
<para>
In the above example, 4 multi channel input files are converted to
one single channel output file. Multi-channel tracks can 
concatenated provided they all have the same number of input channels.

</para><para>

Multiple input files can be made into a multi-channel output file by 
using the -pc option:

</para><para>
<screen>
$ ch_track kdt_010.tr kdt_011.tr kdt_012.tr kdt_013.tr -o -pc longest out.tr
</screen>
</para>
<para>
The argument to -pc can either be longest, in which the output
track is the length of the longest input file, or first in which it
is the length of the first input file.

*/

//@{
//@}

/** @name Extracting channels from multi-channel tracks

The -c option is used to specify channels which should be extracted
from the input.  If the input is a 4 channel track,
</para><para>
<screen>
$ ch_track kdt_m.tr -o a.tr -c "0 2"
</screen>
</para>
<para>
will extract the 0th and 2nd channel (counting starts from 0). The
argument to -c can be either a single number of a list of numbers
(wrapped in quotes).

 */
//@{
//@}


/** @name Extracting of a single region from a track

There are several ways of extracting a region of a track. The
simplest way is by using the start, end, to and from commands to
delimit a sub portion of the input track. For example
</para><para>
<screen>
$ ch_track kdt_010.tr -o small.tr -start 1.45 -end 1.768
</screen>
</para>
<para>
extracts a subtrack starting at 1.45 seconds and extending to 1.768 seconds.
alternatively,
</para><para>
<screen>
$ ch_track kdt_010.tr -o small.tr -from 50 -to 100
</screen>
</para>
<para>
extracts a subtrack starting at 50 frames and extending to 100
frames. Times and frames can be mixed in sub-track extraction. The
output track will have the same number of channels as the input track.


*/
//@{
//@}

/** @name Adding headers and format conversion

It is usually a good idea for all track files to have headers as this
way different files can be handled safely. ch_track provides a means
of adding headers to unheadered files. These files are assumed to
be ascii floats with one channel per line.

The following adds a header to an ascii file.
</para>
<para>
<screen>
$ ch_track kdt_010.atr -o kdt_010.h5.tr -otype est -s 0.01
</screen>
</para>
<para>
ch_track can change the frame shift of a fixed frame file, or convert
a variable frame shift file into a fixed frame shift.  At present this
is done with a very crude resampling technique and hence the output
file may suffer from anti-aliasing distortion.</para><para>


Change to a frame spacing of 0.02 seconds:
</para><para>
<screen>
$ ch_track kdt_010.tr -o kdt_010.tr2 -S 0.02
</screen>
*/
  //@{
  //@}

//@}

