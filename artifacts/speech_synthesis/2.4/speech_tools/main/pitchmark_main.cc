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
/*                    Date   :  1997, 1998, 1999                         */
/*-----------------------------------------------------------------------*/
/*                    Pitchmarking program                               */
/*************************************************************************/

#include <cstdlib>
#include <iostream>
#include <fstream>
#include "EST_unix.h"
#include "EST_cmd_line_options.h"
#include "EST_cmd_line.h"
#include "EST_speech_class.h"
#include "sigpr/EST_pitchmark.h"


void set_options(EST_Features &op, EST_Option &al);

static EST_write_status save_msec(EST_Track &pm, EST_String filename);
static EST_write_status save_ogi_bin(EST_Track &pm, EST_String filename, 
				     int sr);
void pm_to_label(EST_Track &pm, EST_Relation &lab);


/*void pm_to_label(EST_Track &pm, EST_Relation &lab);
void find_pm(EST_Wave &sig, EST_Track &pm);

void pm_min_check(EST_Track &pm, float min);
void pm_sanity_check(EST_Track &pm, float new_end,
		     float max, float min, float def);

void pm_fill(EST_Track &pm, float new_end, float max, 
	     float min, float def);

void pm_to_f0(EST_Track &pm, EST_Track &f0);
*/


/** @name <command> pitchmark </command> <emphasis> Find instants of glottal closure in Laryngograph file</emphasis>

  * @id pitchmark-manual
  * @toc */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**
<command>pitchmark</command> locates instants of glottal closure in a
laryngograph waveform, and performs post-processing to produce even
pitchmarks. EST does not currently provide any means of pitchmarking a
speech waveform.

Pitchmarking is performed by calling the
<function>pitchmark()</function> function, which carries out the
following operations: 

<orderedlist> <listitem><para>Double low pass filter the signal. This
removes noise in the signal. The parameter
<parameter>lx_lf</parameter> specifies the low pass cutoff frequency,
and <parameter>lx_lo</parameter> specifies the order. Double filtering
(feeding the waveform through the filter, then reversing the waveform
and feeding it through again) is performed to reduce any phase shift
between the input and output of the filtering operation.
</para></listitem>

<listitem><para>Double high pass filter the signal. This removes the
very low frequency swell that is often observed in laryngograph
waveforms.  The parameter <parameter>lx_hf</parameter> specifies the high pass cutoff frequency,
and <parameter>lx_ho</parameter> specifies the order.
Double filtering is performed to reduce any phase shift
between the input and output of the filtering operation.
</para></listitem>

<listitem><para>Calculate the delta signal. The filtered waveform is
differentiated using the <function>delta()</function>
function.</para></listitem>

<listitem><para>Low pass filter the delta signal. Some noise may still
be present in the signal, and this is removed by further low pass
filtering. Experimentation has shown that simple mean smoothing is
often more effective than FIR smoothing at this point.  The parameter
<parameter>mo</parameter> is used to specify the size of the mean
smoothing window.  If FIR smoothing is chosen, the parameter
<parameter>df_lf</parameter> specifies the low pass cutoff frequency,
and <parameter>df_lo</parameter> specifies the order. Double filtering
is again used to avoid phase distortion.

</para></listitem>

<listitem><para>Pick zero crossings. Now simple zero-crossing is used
to find the pitchmarks themselves.  </para></listitem>

</orderedlist>

<command>pitchmark</command> also performs post-processing on the pitchmarks. 
This can be used to eliminate pitchmarks which occur too closely together, 
or to provide estimated evenly spaced pitchmarks during unvoiced regions.
The -fill option switches <action>this facility on</action>, 
and -min, -max, -def, 
-end and -wave_end control its operation.

*/

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main (int argc, char *argv[])
{
    EST_Track pm;
    EST_Wave lx;
    EST_Option al;
    EST_Features op;
    EST_String out_file("-");
    EST_StrList files;

    parse_command_line
	(argc, argv, 
       EST_String("[input file] -o [output file] [options]")+
       "Summary: pitchmark laryngograph (lx) files\n"
       "use \"-\" to make input and output files stdin/out\n"
       "-h               Options help\n\n"+
       options_wave_input()+
       options_track_output()+
	 "-lx_lf <int>     lx low frequency cutoff\n\n"
	 "-lx_lo <int>     lx low order\n\n"
	 "-lx_hf <int>     lx high frequency cutoff\n\n"
	 "-lx_ho <int>     lx high order\n\n"
	 "-df_lf <int>     df low frequeny cutoff\n\n"
	 "-df_lo <int>     df low order\n\n"
	 "-med_o <int>     median smoothing order\n\n"
	 "-mean_o <int>    mean smoothing order\n\n"
	 "-inv   Invert polarity of lx signal. Often the lx signal \n"
	 "    is upside down. This option inverts the signal prior  to \n"
	 "    processing.\n\n"
	 "-fill Insert and remove pitchmarks according to min, max\n"
	 "    and def period values. Often it is desirable to place limits\n"
	 "    on the values of the pitchmarks. This option enforces a \n"
	 "    minimum and maximum pitch period (specified by -man and -max).\n"
	 "    If the maximum pitch setting is low enough, this will \n"
	 "    esnure that unvoiced regions have evenly spaced pitchmarks \n\n"
	 "-min <float>  Minimum allowed pitch period, in seconds\n\n"
	 "-max <float>  Maximum allowed pitch period, in seconds\n\n"
	 "-def <float>  Default pitch period in seconds, used for a guide\n"
	 "    as to what length pitch periods should be in unvoiced \n"
	 "    sections \n\n" 
	 "-pm <ifile>   Input is raw pitchmark file. This option is \n"
	 "    used to perform filling operations on an already existing \n"
	 "    set of pitchmarks \n\n"
	 "-f0 <ofile> Calculate F0 from pitchmarks and save to file\n\n"
	 "-end <float> Specify the end time of the last pitchmark, for use \n"
	 "    with the -fill option\n\n"
	 "-wave_end Use the end of a waveform to specify when the \n"
	 "    last pitchmark position should be. The waveform file is only \n"
	 "    read to determine its end, no  processing is performed\n\n"
	 "-inter  Output intermediate waveforms. This will output the \n"
	 "    signal at various stages of processing. Examination of these \n"
	 "    waveforms is extremely useful in setting the parameters for \n"
	 "    similar waveforms\n\n"
	 "-style <string>  \"track\" or \"lab\"\n\n", files, al);

    set_options(op, al);

    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";

    if (!al.present("-pm") || (al.present("-pm") && al.present("-wave_end")))
	if (read_wave(lx, files.first(), al) != read_ok)
	    exit(-1);

    if (al.present("-pm"))
	pm.load(al.val("-pm"));
    else
    {
	if (al.present("-inv"))
	    invert(lx);
	pm = pitchmark(lx, op);
    }

    // this allows the end to be aligned with the end of a waveform
    op.set("pm_end", lx.end());
    
    if (al.present("-f0"))
    {
	EST_Track f0;
	pm_to_f0(pm, f0);
	f0.save(al.val("-f0"));
    }

    // various options for filling he gaps between distant pitchmarks
    // and removing pitchmarks that are too close together

    if (al.present("-fill"))
    {
	pm_fill(pm, op.F("pm_end"), op.F("max_period"), 
			op.F("min_period"), op.F("def_period"));
	pm_fill(pm, op.F("pm_end"), op.F("max_period"), 
			op.F("min_period"), op.F("def_period"));
    }
    else if (al.present("-min"))
	pm_min_check(pm, al.fval("-min"));

    if (al.present("-style"))
    {
	// label format
	if (al.val("-style").contains("lab"))
	{
	    EST_Relation lab;
	    pm_to_label(pm, lab);
	    if (lab.save(out_file + ".pm_lab") != write_ok)
		exit(-1);
	}
	// save file in "traditional" milli-second format
	if (al.val("-style").contains("msec"))
	    save_msec(pm, out_file + ".pm");

	// ogi binary integer sample point format
	if (al.val("-style").contains("ogi_bin"))
	    save_ogi_bin(pm, out_file + ".pmv", lx.sample_rate());
    }
    else if (pm.save(out_file, al.val("-otype", 0)) != write_ok)
    {
	cerr << "pitchmark: failed to write output to \"" 
	    << out_file << "\"" << endl;
	exit(-1);
    }
    return 0;
}

static EST_write_status save_msec(EST_Track &pm, EST_String filename)
{
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
    
    for (int i = 0; i < pm.num_frames(); ++i)
	*outf << pm.t(i)  * 1000.0 << endl;
    
    return write_ok;
}

static EST_write_status save_ogi_bin(EST_Track &pm, EST_String filename, int sr)
{
    int *d;
    FILE *fp;
    int i;
    
    d = new int[pm.num_frames()];
    
    for (i = 0; i < pm.num_frames(); ++i)
	d[i] = int(pm.t(i) * (float) sr);
    
    if ((fp = fopen(filename, "wb")) == NULL)
	return misc_write_error;
    
    if (fwrite(d, pm.num_frames(), sizeof(int), fp) != 1)
    {
	fclose(fp);
	return misc_write_error;
    }
    delete d;
    
    return write_ok;
}

void override_lib_ops(EST_Option &op, EST_Option &al)
{
    op.override_ival("lx_low_frequency", 400);
    op.override_ival("lx_low_order", 19);
    op.override_ival("lx_high_frequency", 40);
    op.override_ival("lx_high_order", 19);
    op.override_ival("df_low_frequency", 1000);
    op.override_ival("df_low_order", 19);
    op.override_fval("min_period", 0.003);
    op.override_fval("max_period", 0.02);
    op.override_fval("def_period", 0.01);
    op.override_fval("pm_end", -1.0);
    
    if (al.present("-lx_lf"))
	op.override_ival("lx_low_frequency", al.ival("-lx_lf", 0));
    if (al.present("-lx_lo"))
	op.override_ival("lx_low_order", al.ival("-lx_lo", 0));
    if (al.present("-lx_hf"))
	op.override_ival("lx_high_frequency", al.ival("-lx_hf", 0));
    if (al.present("-lx_ho"))
	op.override_ival("lx_high_order", al.ival("-lx_ho", 0));
    if (al.present("-med_o"))
	op.override_ival("median_order", al.ival("-med_o", 0));
    if (al.present("-mean_o"))
	op.override_ival("mean_order", al.ival("-mean_o", 0));
    if (al.present("-df_lf"))
	op.override_ival("df_low_frequency", al.ival("-df_lf", 0));
    if (al.present("-df_lo"))
	op.override_ival("df_low_order", al.ival("-df_lo", 0));
    if (al.present("-min"))
	op.override_fval("min_period", al.fval("-min", 0));
    if (al.present("-max"))
	op.override_fval("max_period", al.fval("-max", 0));
    if (al.present("-def"))
	op.override_fval("def_period", al.fval("-def", 0));
    if (al.present("-end"))
	op.override_fval("pm_end", al.fval("-end", 0));
    if (al.present("-inter"))
	op.override_ival("pm_debug", 1);
}

void set_options(EST_Features &op, EST_Option &al)
{
    op.set("lx_low_frequency", LX_LOW_FREQUENCY);
    op.set("lx_low_order", LX_LOW_ORDER);
    op.set("lx_high_frequency", LX_HIGH_FREQUENCY);
    op.set("lx_high_order", LX_HIGH_ORDER);
    op.set("df_low_frequency", DF_LOW_FREQUENCY);
    op.set("df_low_order", DF_LOW_ORDER);
    op.set("min_period", MIN_PERIOD);
    op.set("max_period", MAX_PERIOD);
    op.set("def_period", DEF_PERIOD);
    op.set("pm_end", PM_END);
    
    if (al.present("-lx_lf"))
	op.set("lx_low_frequency", al.ival("-lx_lf", 0));
    if (al.present("-lx_lo"))
	op.set("lx_low_order", al.ival("-lx_lo", 0));
    if (al.present("-lx_hf"))
	op.set("lx_high_frequency", al.ival("-lx_hf", 0));
    if (al.present("-lx_ho"))
	op.set("lx_high_order", al.ival("-lx_ho", 0));
    if (al.present("-med_o"))
	op.set("median_order", al.ival("-med_o", 0));
    if (al.present("-mean_o"))
	op.set("mean_order", al.ival("-mean_o", 0));
    if (al.present("-df_lf"))
	op.set("df_low_frequency", al.ival("-df_lf", 0));
    if (al.present("-df_lo"))
	op.set("df_low_order", al.ival("-df_lo", 0));
    if (al.present("-min"))
	op.set("min_period", al.fval("-min", 0));
    if (al.present("-max"))
	op.set("max_period", al.fval("-max", 0));
    if (al.present("-def"))
	op.set("def_period", al.fval("-def", 0));
    if (al.present("-end"))
	op.set("pm_end", al.fval("-end", 0));
    if (al.present("-inter"))
	op.set("pm_debug", 1);
}

/** @name Examples
</para>
<formalpara><title>Basic Pitchmarking</title>
<para>
<screen>
$ pitchmark kdt_010.lar -o kdt_010.pm -otype est
</screen>
</para> 
</formalpara>

<formalpara><title>Pitchmarking with unvoiced regions
filled</title> <para> The following fills unvoiced regions with pitch
periods that are about 0.01 seconds long. It also post-processes the
set of pitchmarks and ensures that noe are above 0.02 seconds long and
none below 0.003. A final unvoiced region extending to the end of the
wave is specified by using the -wave_end option.
</para> </formalpara><para>
<screen>
$ pitchmark kdt_010.lar -o kdt_010.pm -otype est -fill -min 0.003  \
       -max 0.02 -def 0.01 -wave_end
</screen>

*/

//@{
//@}
