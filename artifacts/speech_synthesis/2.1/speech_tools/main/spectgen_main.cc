/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                       Copyright (c) 1995,1996                         */
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
/*                       Author: Paul Taylor                             */
/*                       Date  : April 1995                              */
/*-----------------------------------------------------------------------*/
/*                     Generate feature vectors                          */
/*                                                                       */
/*=======================================================================*/

#include "EST.h"
#include "EST_cmd_line_options.h"
#include "sigpr/EST_spectrogram.h"

#define DEFAULT_FRAME_SIZE 0.001
#define DEFAULT_FRAME_LENGTH 0.008
#define DEFAULT_ORDER 256
#define DEFAULT_PREEMPH 0.94

void set_options(EST_Features &op, EST_Option &al);

/** @name <command>spectgen</command> <emphasis>Make spectrograms</emphasis>
  * @id spectgen-manual
  * @toc
 */

//@{

/**@name Synopsis
  */
//@{

//@synopsis

/**
spectgen is used to create spectrograms, which are 3d plots of
amplitude against time and frequency. Spectgen takes a waveform and
produces a track, where each channel represents one frequency bin. 

By default spectgen produces a "wide-band" spectrogram, that is one
with high time resolution and low frequency resolution. "Narrow-band"
spectrograms can be produced by using the -shift and -length options.

Typical values for -shift and -length are:



*/

//@}

/**@name Options
  */
//@{

//@options

//@}


int main(int argc, char *argv[])
{
    EST_String out_file;
    EST_StrList files;
    EST_Option al;
    EST_Features op;
    int orig_sr;

    EST_Wave sig;
    EST_Track spec;
    
    parse_command_line
	(argc, argv,
	 EST_String("[input file] -o [output file]\n")+
	 "Summary: make spectrogram\n"+
	 "use \"-\" to make input and output files stdin/out\n"+
	 "-h               Options help\n"+
	 options_wave_input()+
	 "\n"+
	 options_track_output()+
	 "-shift <float> frame spacing in seconds for fixed frame analysis. This \n"
	 "    doesn't have to be the same as the output file spacing - the \n"
	 "    S option can be used to resample the track before saving \n"
	 "    default: "+ftoString(DEFAULT_FRAME_SIZE) +"\n\n"
	 "-length <float>       input frame length in milliseconds\n"+
	 "-sr  <float>  range in which output values should lie\n"+
	 "-slow		slow FFT code\n"+	 
	 "-w  <float>   white cut off (0.0 to 1.0)\n"+
	 "-b  <float>   black cut off (0.0 to 1.0)\n"+
	 "-raw          Don't perform any scaling\n"+
	 "-order <int>     cepstral order\n", files, al);

    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";
    set_options(op, al);

    if (read_wave(sig, files.first(), al) != format_ok)
	exit(-1);
    orig_sr = sig.sample_rate();

    make_spectrogram(sig, spec, op);
    
    spec.save(out_file, al.val("-otype", 0));

    return 0;
}

void set_options(EST_Features &op, EST_Option &al)
{ 
    op.set("frame_shift", DEFAULT_FRAME_SIZE);
    op.set("frame_length", DEFAULT_FRAME_LENGTH);
    op.set("preemph", DEFAULT_PREEMPH);
    op.set("frame_order", DEFAULT_ORDER);
    
    if (al.present("-shift"))
	op.set("frame_shift", al.fval("-shift"));

    if (al.present("-length"))
	op.set("frame_length", al.fval("-length"));

    if (al.present("-order"))
	op.set("frame_order", al.fval("-order"));

    if (al.present("-sr"))
	op.set("sp_range", al.fval("-sr"));

    if (al.present("-w"))
	op.set("sp_wcut", al.fval("-w"));

    if (al.present("-b"))
	op.set("sp_bcut", al.fval("-b"));

    if (al.present("-preemph"))
	op.set("preemph", al.fval("-preemph", 1));

    if (al.present("-raw"))
	op.set("raw", 1);
}
