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
/*                      Author :  Paul Taylor                            */
/*                      Date   :  April 1995                             */
/*-----------------------------------------------------------------------*/
/*                           Filter signals                              */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <iostream>
#include <cmath>
#include "EST_Wave.h"
#include "EST_cmd_line.h"
#include "EST_cmd_line_options.h"
#include "EST_sigpr.h"
#include "EST_wave_aux.h"

void inv_filter_ola(EST_Wave &sig, EST_Track &lpc, EST_Wave &res);
void frame_filter(EST_Wave &sig, EST_Track &lpc, EST_Wave &res);
void inv_lpc_filter_ola(EST_Wave &in_sig, EST_Track &lpc, EST_Wave &out_sig);

void FIR_double_filter(EST_Wave &in_sig, EST_Wave &out_sig, 
		       const EST_FVector &numerator);

/** @name <command>sigfilter</command> <emphasis>Filter waveforms</emphasis>
  * @id sigfilter-manual
  * @toc
 */

//@{

/**@name Synopsis
  */
//@{

//@synopsis

/**
<command>sigfilter</command> filters an input waveform and produces a 
output waveform.

*/

//@}

/**@name Options
  */
//@{

//@options

//@}

int main (int argc, char *argv[])
{
    EST_Wave sig, fsig;
    EST_String in_file("-"), out_file("-"), op_file(""), test;
    EST_Option al;
    EST_TList<EST_String> files;
    EST_Track filter;

    parse_command_line
	(argc, argv, 
	 EST_String("[input file0] -o [output file]\n") + 
	 "Summary: filter waveform files\n"
	 "use \"-\" to make input and output files stdin/out\n"
	 "-h               Options help\n"+
	 options_wave_input()+ "\n"+
	 options_wave_output()+ "\n"
	 "-scale <float> Scaling factor. Increase or descrease the amplitude\n"
	 "    of the whole waveform by the factor given\n\n"

	 "-scaleN <float>  Scaling factor with normalization. \n"
	 "    The waveform is scaled to its maximum level, after which \n"
	 "    it is scaled by the factor given\n\n"
	 "-double Perform double filtering by applying a forward filter,\n"
	 "    then a backward filter, leaving the filtered signla in phase\n"
	 "    with the original\n\n"
	 "-lpfilter <int>  Low pass filter, with cutoff frequency in Hz \n"
	 "    Filtering is performed by a FIR filter which is built at run \n"
	 "    time. The order of the filter can be given by -forder. The \n"
	 "    default value is 199\n\n"

	 "-hpfilter <int>  High pass filter, with cutoff frequency in Hz \n"
	 "    Filtering is performed by a FIR filter which is \n"
	 "    built at run time. The order of the filter can \n"
	 "    be given by -forder. The default value is 199.\n\n"

	 "-forder <int>  Order of FIR filter used for lpfilter and \n"
	 "    hpfilter. This must be ODD. Sensible values range \n"
	 "    from 19 (quick but with a shallow rolloff) to 199 \n"
	 "    (slow but with a steep rolloff). The default is 199.\n\n"
	 "-lpcfilter <ifile> Track file containing lpc filter coefficients\n\n"
	 "-firfilter <ifile> File containing a single set of FIR filter\n"
	 "    coefficients\n\n"
	 "-inv_filter use filter coefficients for inverse filtering\n\n",
	 files, al);

    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";

    if (read_wave(sig, files.first(), al) != format_ok)
	exit(-1);

    if (al.present("-s"))	// rescale
    {
	float scale = al.fval("-s", 0);
	sig.rescale(scale);
    }
    else if (al.present("-scaleN"))	// rescale
    {
	float scale = al.fval("-scaleN", 0);
	if ((scale < 0) || (scale > 1.0))
	{
	    cerr << "ch_wave: -scaleN must be in range 0 to 1" << endl;
	    exit(-1);
	}
	sig.rescale(scale,1);
    }

    // default is to filter before any resampling etc.
    // (this may cause problems for multiplexed data !)

    if (al.present("-lpfilter"))
    {
	FIRlowpass_filter(sig, al.ival("-lpfilter"),al.ival("-forder"));
	fsig = sig;
    }
    if (al.present("-hpfilter"))
    {
	FIRhighpass_filter(sig,al.ival("-hpfilter"),al.ival("-forder"));
	fsig = sig;
    }

    if (al.present("-lpcfilter"))
    {
	filter.load(al.val("-lpcfilter"));
	if (al.present("-inv_filter"))
	    inv_lpc_filter_ola(sig, filter, fsig);
	else
//	    frame_filter(sig, filter, fsig);
	    cout << "not done yet\n";
    }
    if (al.present("-firfilter"))
    {
	EST_FVector firfilter;
	firfilter.load(al.val("-firfilter"));
	if (al.present("-double"))
	    FIR_double_filter(sig, fsig, firfilter);
	else
	    FIRfilter(sig, fsig, firfilter);
    }

    if (write_wave(fsig, out_file, al) != write_ok)
    {
	cerr << "sigfilter: failed to write output to \"" << out_file 
	    << "\"" << endl;
	exit(-1);
    }
    return 0;
}
