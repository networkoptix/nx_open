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
/*                      Authors: Paul Taylor and Simon King              */
/*                       Date  : April 1995                              */
/*-----------------------------------------------------------------------*/
/*                     Generate feature vectors                          */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include "EST_speech_class.h"
#include "EST_string_aux.h"
#include "EST_cmd_line.h"
#include "EST_cmd_line_options.h"
#include "sigpr/EST_sigpr_utt.h"
#include "sigpr/EST_filter.h"

#define EPSILON (0.0001)

#define DEFAULT_FRAME_SIZE 0.01
#define DEFAULT_FRAME_FACTOR 2.0
#define DEFAULT_LPC_ORDER 16
#define DEFAULT_REF_ORDER 16
#define DEFAULT_CEP_ORDER 12
#define DEFAULT_FBANK_ORDER 20
#define DEFAULT_MELCEP_ORDER 12
#define DEFAULT_WINDOW "hamming"
#define DEFAULT_PREEMPH 0
#define DEFAULT_LIFTER 0


// sane values for pitchmarks (in seconds)

#define MINIMUM_PITCH_PERIOD (0.0033) // 300 hz
#define MAXIMUM_PITCH_PERIOD (0.02)   // 50 Hz
#define DEFAULT_PITCH_PERIOD (0.01)   // 100 Hz

void calculate_orders(EST_StrList &clist, EST_IList &olist,
		      EST_Option &op);

void add_channels_to_map(EST_StrList &map, EST_StrList &types, 
			 EST_Features &op, int order);

void set_options(EST_Features &op, EST_Option &al);

EST_String sigpr_options_supported(void)
{
    return
	EST_String("")+ 
	"    lpc      linear predictive coding\n"
	"    cep      cepstrum coding from lpc coefficients\n"
	"    melcep   Mel scale cepstrum coding via fbank\n"
	"    fbank    Mel scale log filterbank analysis\n"
	"    lsf      line spectral frequencies\n"
	"    ref      Linear prediction reflection coefficients\n"
	"    power\n"
	"    f0\n"
	"    energy: root mean square energy\n";
};



/** @name <command>sig2fv</command> <emphasis>Generate signal processing coefficients from waveforms</emphasis>
  * @id sigfv-manual
  * @toc
 */

//@{

/**@name Synopsis
  */
//@{

//@synopsis

/**
sig2fv is used to create signal processing feature vector analysis on speech
waveforms.
The following types of analysis are provided:

<itemizedlist>
<listitem><para>Linear prediction (LPC)</para></listitem>
<listitem><para>Cepstrum coding from lpc coefficients</para></listitem>
<listitem><para>Mel scale cepstrum coding via fbank</para></listitem>
<listitem><para>Mel scale log filterbank analysis</para></listitem>
<listitem><para>Line spectral frequencies</para></listitem>
<listitem><para>Linear prediction reflection coefficients</para></listitem>
<listitem><para>Root mean square energy</para></listitem>
<listitem><para>Power</para></listitem>
<listitem><para>fundamental frequency (pitch)</para></listitem>
<listitem><para>calculation of delta and acceleration coefficients of all of the 
above</para></listitem>
</itemizedlist>

The -coefs option is used to specify a list of the names of what sort
of basic processing is required, and -delta and -acc are used for
delta and acceleration coefficients respectively.

*/

//@}

/**@name Options
  */
//@{

//@options

//@}


int main(int argc, char *argv[])
{
    EST_String out_file("-");
    EST_StrList files;
    EST_Option al;
    EST_Features op;
    EST_Wave sig;
    EST_Track full;
    EST_StrList coef_list, delta_list, acc_list, tlist, map;
    EST_IList olist;

    parse_command_line
	(argc, argv, 
	 EST_String("[input file] -o [output file]\n")+
	 "Summary: generate acoustic feature vectors for a waveform file \n"
	 "use \"-\" to make input and output files stdin/out \n"
	 "-h   Options help \n\n" +
	 options_wave_input() + 
	 options_track_output() + " \n"
	 "-shift <float> frame spacing in seconds for fixed frame analysis. This \n"
	 "    doesn't have to be the same as the output file spacing - the \n"
	 "    S option can be used to resample the track before saving \n"
	 "    default: "+ftoString(DEFAULT_FRAME_SIZE) +"\n\n"
	 "-factor <float> Frames lengths will be FACTOR times the \n"
	 "    local pitch period. \n"
	 "    default: "+ftoString(DEFAULT_FRAME_FACTOR) +"\n\n"
	 "-pm <ifile>  Pitch mark file name. This is used to \n"
	 "    specify the positions of the analysis frames for pitch \n"
	 "    synchronous analysis. Pitchmark files are just standard \n"
	 "    track files, but the channel information is ignored and \n"
	 "    only the time positions are used\n"
	 "-size <float> If specified with pm, size is used as the \n"
         "    fixed window size (times factor) rather than size within \n"
         "    each the pms.\n\n"

	 "-coefs <string> list of basic types of processing required. \n"
	 "    Permissable types are: \n" + sigpr_options_supported()+" \n"
	 "-delta <string> list of delta types of processing required. Basic \n"
	 "    processing does not need to be specified for this option to work. \n"
	 "    Permissable types are: \n" + sigpr_options_supported()+" \n"
	 "-acc <string>  list of acceleration (delta delta) processing \n"
	 "    required. Basic processing does not need to be specified for \n"
         "    this option to work. \n" 
	 "    Permissable types are: \n" 
	 + sigpr_options_supported()+"\n"
	 "-window_type <string> Type of window used on waveform. \n"
	 "    Permissable types are: \n" +
	 EST_Window::options_supported() + 
	 "    default: "DEFAULT_WINDOW"\n\n"
	 "-lpc_order <int>      Order of lpc analysis. \n\n"
	 "-ref_order <int>      Order of lpc reflection coefficient analysis. \n\n"
	 "-cep_order <int>      Order of lpc cepstral analysis.\n\n"
	 "-melcep_order <int>   Order of Mel cepstral analysis.\n\n"
	 "-fbank_order <int>    Order of filter bank analysis.\n\n"
	 "-preemph <float>      Perform pre-emphasis with this factor.\n\n"
	 "-lifter <float>       lifter coefficient.\n\n"
	 "-usepower             use power rather than energy in filter bank \n"
	 "    analysis\n\n"+
	 "-include_c0           include cepstral coefficient 0\n\n"
	 "-order <string>       order of analyses\n", files, al);

    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";
    set_options(op, al);

    StringtoStrList(al.val("-coefs"), coef_list);
    StringtoStrList(al.val("-delta"), delta_list);
    StringtoStrList(al.val("-acc"), acc_list);

    StringtoStrList(al.val("-order"), tlist);
    StrListtoIList(tlist, olist);
    
    if (read_wave(sig, files.first(), al) != read_ok)
	exit(-1);

    // allocate and fill time axis
    if (al.present("-pm"))
    {
	if (read_track(full, al.val("-pm"), al))
	    exit(1);
    }
    else
    {
	full.resize((int)ceil(sig.end() / op.F("frame_shift")), 0);
	full.fill_time(op.F("frame_shift"));
    }

    // allocate channels
    add_channels_to_map(map, coef_list, op, 0);
    add_channels_to_map(map, delta_list, op, 1);
    add_channels_to_map(map, acc_list, op, 2);

    //cerr << "MAP " << map << endl;

    full.resize(EST_CURRENT, map);

    if (al.present("-preemph"))
	pre_emphasis(sig, al.fval("-preemph"));

    if(al.present("-usepower"))
	cerr << "sig2fv: -usepower currently not supported" << endl;

    sigpr_base(sig, full, op, coef_list);
    sigpr_delta(sig, full, op, delta_list);
    sigpr_acc(sig, full, op, acc_list);

    if (al.present("-S"))
    {
	cout << "-S " << al.fval("-S") << endl;
	full.sample(al.fval("-S"));
    }

    if (full.save(out_file, al.val("-otype", 0)) != write_ok)
    {
	cerr << "sig2fv: failed to write output to \"" << out_file 
	    << "\"" << endl;
	exit(-1);
    }
    return 0;
}



void calculate_orders(EST_StrList &clist, EST_IList &olist,
		      EST_Option &op)
{
    EST_Litem *c, *o;
    EST_String k;
    int v;

    for (c = clist.head(), o = olist.head(); c && o; c= c->next(), o = o->next())
    {
	k = clist(c) + "_order";
	v = olist(o);
	op.override_ival(k, v);
    }
}

void set_options(EST_Features &op, EST_Option &al)
{ 
    op.set("frame_shift", DEFAULT_FRAME_SIZE);
    op.set("frame_factor", DEFAULT_FRAME_FACTOR);
    op.set("window_type", DEFAULT_WINDOW); 

    op.set("preemph", DEFAULT_PREEMPH);
    op.set("lifter", DEFAULT_LIFTER);

    op.set("lpc_order", DEFAULT_LPC_ORDER);
    op.set("ref_order", DEFAULT_REF_ORDER);
    op.set("cep_order", DEFAULT_CEP_ORDER);
    op.set("fbank_order", DEFAULT_FBANK_ORDER);
    op.set("melcep_order", DEFAULT_MELCEP_ORDER);

    op.set("max_period", MAXIMUM_PITCH_PERIOD);
    op.set("min_period", MINIMUM_PITCH_PERIOD);
    op.set("def_period", DEFAULT_PITCH_PERIOD);
    
    if (al.present("-max_period"))
	op.set("max_period", al.fval("-max_period", 0));
    if (al.present("-min_period"))
	op.set("min_period", al.fval("-min_period", 0));
    if (al.present("-def_period"))
	op.set("def_period", al.fval("-def_period", 0));

    if (al.present("-window_type"))
	op.set("window_type", al.sval("-window_type", 1));
    
    if (al.present("-shift"))
	op.set("frame_shift", al.fval("-shift", 1));
    if (al.present("-factor"))
	op.set("frame_factor", al.fval("-factor", 1));
    if (al.present("-size"))
	op.set("frame_factor", op.F("frame_factor")*-1.0*al.fval("-size"));
    if (al.present("-length"))
	op.set("frame_factor", 
			 al.fval("-length", est_errors_allowed)/op.F("frame_shift",est_errors_allowed));
    
    if (al.present("-preemph"))
	op.set("preemph", al.fval("-preemph", 1));
    if (al.present("-lifter"))
	op.set("lifter", al.fval("-lifter", 1));

    if (al.present("-lpc_order"))
	op.set("lpc_order", al.ival("-lpc_order", 1));
    if (al.present("-ref_order"))
	op.set("ref_order", al.ival("-ref_order", 1));
    if (al.present("-cep_order"))
	op.set("cep_order", al.ival("-cep_order", 1));
    if (al.present("-fbank_order"))
	op.set("fbank_order", al.ival("-fbank_order", 1));
    if (al.present("-melcep_order"))
	op.set("melcep_order", al.ival("-melcep_order", 1));

    if (al.present("-usepower"))
	op.set("usepower", al.val("-usepower", 1));

    if (al.present("-include_c0"))
	op.set("include_c0", al.val("-include_c0", 1));

}

/**@name Examples


Fixed frame basic linear prediction:

To produce a set of linear prediction coefficients at every 10ms, using
pre-emphasis  and saving in EST format:

<para>
<screen>
$ sig2fv kdt_010.wav -o kdt_010.lpc -coefs "lpc" -otype est -shift 0.01 -preemph 0.5
</screen>
</para>
<formalpara><title>
Pitch Synchronous linear prediction</title><para>. The following used the set of pitchmarks
in kdt_010.pm as the centres of the analysis windows.
</para>
</formalpara>

<para>
<screen>
$ sig2fv kdt_010.wav -pm kdt_010.pm -o kdt_010.lpc -coefs "lpc" -otype est -shift 0.01 -preemph 0.5
</screen>
</para>

<para>
F0, Linear prediction and cepstral coefficients:

<screen>
$ sig2fv kdt_010.wav -o kdt_010.lpc -coefs "f0 lpc cep" -otype est -shift 0.01
</screen>

Note that pitchtracking can also be done with the
<command>pda</command> program. Both use the same underlying
technique, but the pda program offers much finer control over the
pitch track specific processing parameters.

</para>

<para>Energy, Linear Prediction and Cepstral coefficients, with a 10ms frame shift
during analysis but a 5ms frame shift in the output file:

<para>
<screen>
$ sig2fv kdt_010.wav -o kdt_010.lpc -coefs "f0 lpc cep" -otype est -S 0.005
      -shift 0.01
</screen>
</para>

<para>Delta  and acc coefficients can be calculated even if their base form is not 
required. This produces normal energy coefficients and cepstral delta coefficients:

<para>
<screen>
$ sig2fv ../kdt_010.wav -o kdt_010.lpc -coefs "energy" -delta "cep" -otype est
</screen>
</para>

<para>Mel-scaled cepstra, Delta and acc coefficients, as is common in speech 
recognition:
<para>
<screen>
$ sig2fv ../kdt_010.wav -o kdt_010.lpc -coefs "melcep" -delta "melcep" -acc "melcep" -otype est -preemph 0.96
</screen>

*/
//@{
//@}



//@}
