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
/*                      Author :  Paul Taylor, Simon King                */
/*                      Date   :  1995-99                                */
/*-----------------------------------------------------------------------*/
/*                         Design FIR filter                             */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <iostream>
#include <cmath>
#include "EST_Wave.h"
#include "EST_cmd_line.h"
#include "EST_cmd_line_options.h"
#include "sigpr/EST_filter_design.h"

/** @name <command>design_filter</command>
  * @id designfilter-manual
  * @toc
 */

//@{

/**@name Synopsis
  */
//@{

//@synopsis

/**
<command>designfilter</command> computes the coefficients of a FIR
filter with a given frequency response. The user supplies the
frequency response as a vector of evenly spaced gains ranging from 0
to half the sampling frequency. The length of this vector must be a
power of 2. The filter coefficients can be used by the \Ref{sigfilter}
program.

*/

//@}

/**@name Options
  */
//@{

//@options

//@}

int main (int argc, char *argv[])
{
    EST_FVector fresponse, filter;
    EST_String in_file("-"), out_file("-"), op_file(""), test;
    EST_Option al;
    EST_TList<EST_String> files;
    int forder;

    parse_command_line
	(argc, argv, 
	 EST_String("[input file0] -o [output file]\n") + 
	 "Summary: filter waveform files\n"
	 "use \"-\" to make input and output files stdin/out\n"
	 "-h               Options help\n"
	 "-forder <int>  Order of FIR filter. This must be ODD.\n"
	 "    Sensible values range \n"
	 "    from 19 (quick but with a shallow rolloff) to 199 \n"
	 "    (slow but with a steep rolloff). The default is 199.\n\n"
	 "-double  Design a filter suitable for double (zero-phase)\n"
         "    filtering\n\n"
	 "-o <ofile> output filter file\n",
	 files, al);

    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";

    if (fresponse.load(files.first()) != format_ok)
	exit(-1);

    forder = al.present("-forder") ? al.ival("-forder") : 199;

    if (al.present("-double"))
	for (int i = 0; i < fresponse.length(); i++)
	    fresponse[i] = sqrt(fresponse[i]);

    // user gives freq response for freq range 0...half sampling freq
    // we need to make a mirror image of this
    
    int l = fresponse.length() * 2;
    EST_FVector full_fresponse(l);

    for(int i = 0;i<fresponse.length();i++)
      {
	full_fresponse[i] = fresponse(i);
	full_fresponse[l-1-i] = fresponse(i);
      }

    filter = design_FIR_filter(full_fresponse, forder);
    filter.save(out_file, "est_ascii");
}

/** @name Example

<title>Designing a bandpass filter</title>

The frequency response vector must be placed in a file, in either
ascii of EST headered format. For example:
<screen>
<para>EST_File fvector</para>
<para>version 1</para>
<para>DataType ascii</para>
<para>length 128</para>
<para>EST_Header_End</para>
<para>0.0</para>
<para>0.0</para>
<para>.....[etc]</para>
<para>1.0</para>
<para>1.0</para>
<para>1.0</para>
<para>.....[etc]</para>
<para>0.0</para>
<para>0.0</para>
<para>0.0</para>
<para>.....[etc]</para>
</screen>
And the filter is simply designed using
</para>
<para>
<screen>
$ design_filter -o filter.coefficients filter.freq_response
</screen>
</para>
<para>
where filter.freq_response is the above file, and filter.coefficients
is the output file which can be used by \Ref{sigfilter}.
</para>

*/
  //@{
  //@}

//@}
