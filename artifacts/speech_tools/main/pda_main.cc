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
/*                    Author :  Paul Taylor                              */
/*                    Date   :  May 1994                                 */
/*-----------------------------------------------------------------------*/
/*             Pitch Detection Algorithm Main routine                    */
/*                                                                       */
/*=======================================================================*/
#include <fstream>
#include "EST.h"
#include "sigpr/EST_sigpr_utt.h"
#include "EST_cmd_line_options.h"

void set_parameters(EST_Features &a_list, EST_Option &al);

void option_override(EST_Features &op, EST_Option al, 
		     const EST_String &option, const EST_String &arg);

static int save_pm(EST_String filename, EST_Track fz);

/** @name <command>pda</command> <emphasis>Pitch Detection Algorithm</emphasis>
    @id pda-manual
  * @toc
 */

//@{

/**@name Synopsis
  */
//@{

//@synopsis

/**
pda is a pitch detection algorithm that produces a fundamental frequency
contour from a speech waveform file. At present only the
super resolution pitch determination algorithm is implemented.
See (Medan, Yair, and Chazan, 1991) and (Bagshaw et al., 1993) for a detailed
description of the algorithm.
</para><para>

The default values given below were found to optimise the performance
of the pitch determination algorithm for speech data sampled at 20kHz
using a 16\-bit waveform and low pass filter with a 600Hz cut-off
frequency and more than \-85dB rejection above 700Hz. The best
performances occur if the [\-p] flag is passed.  </para><para>
*/

//@}

/**@name Options
  */
//@{

//@options

//@}


int main (int argc, char *argv[])
{
    EST_Track fz;
    EST_Wave sig;
    EST_Option al;
    EST_Features op;
    EST_String out_file("-");
    EST_StrList files;
    
    parse_command_line
	(argc, argv, 
       EST_String("[input file] -o [output file] [options]\n")+
       "Summary: pitch track waveform files\n"
       "use \"-\" to make input and output files stdin/out\n"
       "-h               Options help\n\n"+
       options_wave_input()+
       options_pda_general()+
       options_pda_srpd()+
       options_track_output(),
			files, al);

    default_pda_options(op);
    set_parameters(op, al);

    if (read_wave(sig, files.first(), al) != format_ok)
	exit(-1);

    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";

    pda(sig, fz, op);		// do f0 tracking

    if (al.present("-pm"))
	save_pm(out_file, fz);
    else
	fz.save(out_file, op.S("f0_file_type", "0"));

    if (al.present("-diff"))
    {
	fz = differentiate(fz);
	fz.save(out_file + ".diff", op.S("f0_file_type", "0"));
    }
    return 0;
}


void set_parameters(EST_Features &op, EST_Option &al)
{
    op.set("srpd_resize", 1);

    // general options
    option_override(op, al, "pda_frame_shift", "-shift");
    option_override(op, al, "pda_frame_length", "-length");
    option_override(op, al, "max_pitch", "-fmax");
    option_override(op, al, "min_pitch", "-fmin");

    // low pass filtering options.
    option_override(op, al, "lpf_cutoff", "-u");
    option_override(op, al, "lpf_order", "-forder");

    option_override(op, al, "decimation", "-d");
    option_override(op, al, "noise_floor", "-n");
    option_override(op, al, "min_v2uv_coef_thresh", "-m");
    option_override(op, al, "v2uv_coef_thresh_ratio", "-R");
    option_override(op, al, "v2uv_coef_thresh", "-H");
    option_override(op, al, "anti_doubling_thresh", "-t");
    option_override(op, al, "peak_tracking", "-P");

    option_override(op, al, "f0_file_type", "-otype");
    option_override(op, al, "wave_file_type", "-itype");

    if (al.val("-L", 0) == "true")
	op.set("do_low_pass", "true");
    if (al.val("-R", 0) == "true")
	op.set("do_low_pass", "false");
    

/*    op.set("lpf_cutoff",al.val("-u", 0));
    op.set("lpf_order",al.val("-forder", 0));
    
    //sprd options
    op.set("decimation", al.val("-d", 0));
    op.set("noise_floor",   al.val("-n", 0));
    op.set("min_v2uv_coef_thresh", al.val("-m", 0));
    op.set("v2uv_coef_thresh_ratio", al.val("-r", 0));
    op.set("v2uv_coef_thresh", al.val("-H", 0));
    op.set("anti_doubling_thresh", al.val("-t", 0));
    op.set("peak_tracking", al.val("-P", 0));
    if (al.val("-L", 0) == "true")
	op.set("do_low_pass", "true");
    if (al.val("-R", 0) == "true")
	op.set("do_low_pass", "false");
    op.set("f0_file_type", al.val("-otype", 0));
    op.set("wave_file_type", al.val("-itype", 0));
*/
}

/*    a_list.override_val("sample_rate", al.val("-f", 0));
    a_list.override_val("min_pitch",  al.val("-fmin", 0));
    a_list.override_val("max_pitch",  al.val("-fmax", 0));
    a_list.override_val("pda_frame_shift", al.val("-s", 0));
    a_list.override_val("pda_frame_length",al.val("-l", 0));
    
    // low pass filtering options.
    a_list.override_val("lpf_cutoff",al.val("-u", 0));
    a_list.override_val("lpf_order",al.val("-forder", 0));
    
    //sprd options
    a_list.override_val("decimation", al.val("-d", 0));
    a_list.override_val("noise_floor",   al.val("-n", 0));
    a_list.override_val("min_v2uv_coef_thresh", al.val("-m", 0));
    a_list.override_val("v2uv_coef_thresh_ratio", al.val("-r", 0));
    a_list.override_val("v2uv_coef_thresh", al.val("-H", 0));
    a_list.override_val("anti_doubling_thresh", al.val("-t", 0));
    a_list.override_val("peak_tracking", al.val("-P", 0));
    if (al.val("-L", 0) == "true")
	a_list.override_val("do_low_pass", "true");
    if (al.val("-R", 0) == "true")
	a_list.override_val("do_low_pass", "false");
    a_list.override_val("f0_file_type", al.val("-otype", 0));
    a_list.override_val("wave_file_type", al.val("-itype", 0));
*/


static int save_pm(EST_String filename, EST_Track fz)
{
    ostream *outf;
    float position, period;

    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);

    if (!(*outf))
    {
	cerr << "save_pm: can't write to file \"" << filename << "\"" << endl;
	return -1;
    }

    *outf << "XAO1\n\n";	// xmg header identifier.
    *outf << "LineType        bars \n";
    *outf << "LineStyle       solid \n";
    *outf << "LineWidth       0 \n";
    *outf << "Freq 16\n";
    *outf << "Format  Binary \n";
    *outf << char(12) << "\n";	// control L character

    position = 0.0;
    int gap = 0;
    for (int i = 0; i < fz.num_frames(); ++i)
    {
	if (fz.val(i))
	{
	    if (gap)
	    {
		position = fz.t(i);
		gap = 0;
	    }
	    period = 1.0 / fz.a(i);
	    *outf << (position + period) * 1000.0 << endl;
	    position += period;
	}
	else
	    gap = 1;
    }
    
    if (outf != &cout)
	delete outf;

    return 0;
}

/**@name Examples

Pitch detection on typical male voice, using low pass filtering:
<screen>
$ pda kdt_010.wav -o kdt_010.f0 -fmin 80 -fmax 200 -L
</screen>
*/
//@{

//@}
//@}
