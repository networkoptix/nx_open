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
/*                   Author :  Paul Taylor                               */
/*                   Date   :  April 1994                                */
/*************************************************************************/

#include "EST_speech_class.h"
#include "sigpr/EST_sigpr_utt.h"
#include "sigpr/EST_filter.h"
#include "srpd.h"
#include "EST_error.h"
#include "EST_string_aux.h"

int read_next_wave_segment (EST_Wave &sig, struct Srpd_Op *paras, 
			    SEGMENT_ *p_seg);

static void srpd(EST_Wave &sig, EST_Track &fz, Srpd_Op &srpd_op, int resize);
static struct Srpd_Op *default_srpd_op(struct Srpd_Op *srpd);
static void parse_srpd_list(EST_Features &a_list, struct Srpd_Op *srpd);

void pda(EST_Wave &sig, EST_Track &fz, EST_Features &op, EST_String method)
{
    if (method == "")
    {
	if (op.present("pda_method"))
	    method = op.S("pda_method");
    }
    if (method == "")	
	srpd(sig, fz, op);
    else if  (method == "srpd")
	srpd(sig, fz, op);
    else
	EST_error("Unknown pda %s\n", (const char *)method);
}

void icda(EST_Wave &sig, EST_Track &fz, EST_Track &speech, EST_Features &op, 
	       EST_String method)
{ // intonation contour detection algorithm
    EST_Track raw_fz;
    if (method == "")
    {
	if (op.present("pda_method"))
	    method = op.S("pda_method");
    }
    if (method == "")	
	srpd(sig, raw_fz, op);
    else if  (method == "srpd")
	srpd(sig, raw_fz, op);
    else
	EST_error("Unknown pda %s\n", (const char *)method);

    smooth_phrase(raw_fz, speech, op, fz);
}

void srpd(EST_Wave &sig, EST_Track &fz, EST_Features &op)
{
    Srpd_Op srpd_op;

    default_srpd_op(&srpd_op); // default values
    parse_srpd_list(op, &srpd_op); // override with options

    if (op.I("do_low_pass",0))
	FIRlowpass_filter(sig, op.I("lpf_cutoff"),op.I("lpf_order"));

    srpd(sig, fz, srpd_op, op.I("srpd_resize", 0));
}

/*void do_srpd_fz(EST_Wave &sig, EST_Track &fz)
{
    Srpd_Op srpd_op;
    default_srpd_op(&srpd_op);
    srpd(sig, fz, srpd_op, 1);
}
*/

void srpd(EST_Wave &sig, EST_Track &fz, Srpd_Op &srpd_op, int resize)
{
    int i, rns, tracklen, j = 0;
    SEGMENT_ segment;
    CROSS_CORR_ cc;
    STATUS_ pda_status, held_status;
    srpd_op.sample_freq = sig.sample_rate();
    float min, max;
    min = srpd_op.min_pitch; // must store as set up routines corrupt
    max = srpd_op.max_pitch;

    initialise_structures (&srpd_op, &segment, &cc);
    initialise_status (&srpd_op, &pda_status);
    initialise_status (&srpd_op, &held_status);

    tracklen = (sig.num_samples() - segment.length) / segment.shift + 1;

    if (resize)
    {
	fz.set_equal_space(true);
	fz.resize(tracklen, 1);
	fz.set_channel_name("F0", 0);
	fz.fill_time(srpd_op.shift/1000);
    }

    if (!fz.equal_space())
	EST_error("Pitch tracking algorithm must have equal spaced track\n");
    
    while ((rns = read_next_wave_segment (sig, &srpd_op, &segment)) != 0) 
    {
	if (rns == 2) 
	{
	    for (i = 0; i < cc.size; cc.coeff[i++] = 0.0);
	    initialise_status (&srpd_op, &pda_status);
	}
	else
	    super_resolution_pda (&srpd_op, segment, &cc, &pda_status);
	if (pda_status.s_h == HOLD) 
	{
	    held_status.pitch_freq = pda_status.pitch_freq;
	    held_status.v_uv = VOICED;
	    held_status.s_h = HELD;
	    held_status.cc_max = pda_status.cc_max;
	    held_status.threshold = pda_status.threshold;
	    continue;
	}
	if (held_status.s_h == HELD) 
	{
	    if (pda_status.pitch_freq == BREAK_NUMBER) 
	    {
		held_status.pitch_freq = BREAK_NUMBER;
		held_status.v_uv = UNVOICED;
	    }
	    held_status.s_h = SENT;
	    if (held_status.v_uv != VOICED) 
		fz.set_break(j);
	    fz.a(j++) = held_status.pitch_freq;
	    //    printf( "track set:  %d (of %d) to %f\n", j-1, fz.length(), held_status.pitch_freq );
	}
	if (pda_status.v_uv != VOICED) 
	    fz.set_break(j);
	fz.a(j++) = pda_status.pitch_freq;
	//printf( "track set:  %d (of %d) to %f\n", j-1, fz.length(), pda_status.pitch_freq );
    }
    if (held_status.s_h == HELD) 
    {
	held_status.pitch_freq = BREAK_NUMBER;
	held_status.v_uv = UNVOICED;
	fz.set_break(j);
	fz.a(j++) = held_status.pitch_freq;
    }
    end_structure_use (&segment, &cc);
}

static struct Srpd_Op *default_srpd_op(struct Srpd_Op *srpd)
{ 
    srpd->L = DEFAULT_DECIMATION;
    srpd->min_pitch = DEFAULT_MIN_PITCH;
    srpd->max_pitch = DEFAULT_MAX_PITCH;
    srpd->shift = DEFAULT_SHIFT;
    srpd->length = DEFAULT_LENGTH;
    srpd->Tsilent = DEFAULT_TSILENT;
    srpd->Tmin = DEFAULT_TMIN;
    srpd->Tmax_ratio = DEFAULT_TMAX_RATIO;
    srpd->Thigh = DEFAULT_THIGH;
    srpd->Tdh = DEFAULT_TDH;
    srpd->make_ascii = 0;
    srpd->peak_tracking = 0;
    srpd->sample_freq = DEFAULT_SF;
      /* p_par->Nmax and p_par->Nmin cannot be initialised */
    return(srpd);
}

static void parse_srpd_list(EST_Features &al, struct Srpd_Op *srpd)
{ 
    if (al.present("decimation"))
	srpd->L = al.I("decimation");
    if (al.present("min_pitch"))
	srpd->min_pitch = al.F("min_pitch");
    if (al.present("max_pitch"))
	srpd->max_pitch = al.F("max_pitch");    
    if (al.present("pda_frame_shift"))
	srpd->shift = al.F("pda_frame_shift") * 1000.0;
    if (al.present("pda_frame_length"))
	srpd->length = al.F("pda_frame_length") * 1000.0;
    if (al.present("noise_floor"))
	srpd->Tsilent = al.I("noise_floor");
    if (al.present("v2uv_coeff_thresh"))
	srpd->Thigh = al.F("v2uv_coef_thresh");
    if (al.present("min_v2uv_coef_thresh"))
	srpd->Tmin = al.F("min_v2uv_coef_thresh");
    if (al.present("v2uv_coef_thresh_ratio"))
	srpd->Tmax_ratio = al.F("v2uv_coef_thresh_ratio");
    if (al.present("anti_doubling_thresh"))
	srpd->Tdh = al.F("anti_doubling_thresh");
    if (al.present("peak_tracking"))
	srpd->peak_tracking = al.I("peak_tracking");
    if (al.present("sample_frequency"))
	srpd->sample_freq = al.I("sample_frequency");
}

void default_pda_options(EST_Features &al)
{
    al.set("min_pitch", "40.0");
    al.set("max_pitch", "400.0");
    al.set("pda_frame_shift", "0.005");
    al.set("pda_frame_length", DEFAULT_LENGTH / 1000.0);
    al.set("lpf_cutoff", "600");
    al.set("lpf_order", "49");
    al.set("f0_file_type", "esps");
    al.set("decimation", DEFAULT_DECIMATION);
    al.set("noise_floor", DEFAULT_TSILENT);
    al.set("min_v2uv_coef_thresh", DEFAULT_TMIN);
    al.set("v2uv_coef_thresh_ratio", DEFAULT_TMAX_RATIO);
    al.set("v2uv_coef_thresh", DEFAULT_THIGH);
    al.set("anti_doubling_thresh", DEFAULT_TDH);
    al.set("peak_tracking", 0);
}

EST_String options_pda_general(void)
{
    // The standard waveform input options 
    return
	EST_String("")+
	"-L  Perform low pass filtering on input. This option should always \n"
	"    be used in normal processing as it usually increases \n"
	"    performance considerably\n\n"
	"-P  perform peak tracking\n\n" 
	"-fmin <float> miniumum F0 value. Sets the minimum allowed F0 in \n" 
	"    output track. Default is "+ftoString(DEFAULT_MIN_PITCH)+".\n "
	"    Changing this to suit the speaker usually increases  \n"
	"    performance. Typical recommended values are 60-90Hz for\n"
	"    males and 120-150Hz  for females\n\n"
	"-fmax <float> maxiumum F0 value. Sets the maximum allowed F0 in \n" 
	"    output track. Default is "+ftoString(DEFAULT_MAX_PITCH)+". \n"
	"    Changing this to suit the speaker usually increases \n"
	"    performance. Typical recommended values are 200Hz for \n"
	"    males and 300-400Hz for females\n\n"
	"-shift <float> frame spacing in seconds for fixed frame analysis. \n"
	"    This doesn't have to be the same as the output file spacing - \n"
	"    the -S option can be used to resample the track before saving \n"
	"    default: "+ftoString(DEFAULT_SHIFT/1000.0) +"\n\n"
	"-length <float> analysis frame length in seconds.\n"
	"    default: "+ftoString(DEFAULT_LENGTH/1000.0) +"\n\n"
	"-lpfilter <int>   Low pass filter, with cutoff frequency in Hz \n"
	"    Filtering is performed by a FIR filter which is built at run \n"
	"    time. The order of the filter can be given by -forder. The \n"
	"    default value is 199\n\n"
	"-forder <int>  Order of FIR filter used for lpfilter and \n"
	"    hpfilter. This must be ODD. Sensible values range \n"
	"    from 19 (quick but with a shallow rolloff) to 199 \n"
	"    (slow but with a steep rolloff). The default is 199.\n\n";
}

EST_String options_pda_srpd(void)
{
    // The standard waveform input options 
    return
	EST_String("")+
	"-d <float> decimation factor\n"
	"    set down-sampling for quicker computation so that only one in \n"
	"    <parameter>decimation factor</parameter> samples are used in the first instance. \n"
	"    Must be in the range of one to ten inclusive. Default is four. \n"
	"    For data sampled at 10kHz, it is advised that a decimation \n"
	"    factor of two isselected.\n\n"

	"-n <float> Inoise floor.\n"
	"    Set the maximum absolute signal amplitude that represents  \n"
	"    silence to <parameter>Inoise floor</parameter>. If the absolute amplitude of \n"
	"    the first segment in a given frame is below this level at all \n"
	"    times, then the frame is classified as representing silence. \n"
	"    Must be a positive number. Default is 120 ADC units.\n\n"

	"-H <float> unvoiced to voiced coeff threshold\n"
	"    set the correlation coefficient threshold which must be \n"
	"    exceeded in a transition from an unvoiced classified frame \n"
	"    of speech to a voiced frame as the unvoiced to voiced coeff \n"
	"    threshold. Must be in the range zero to one inclusive. \n"
	"    Default is 0.88.\n\n"

	"-m <float> min voiced to unvoiced coeff threshold \n"
	"    set the minimum allowed correlation coefficient threshold \n"
	"    which must not be exceeded in a transition from a voiced \n"
	"    classified frame of speech to an unvoiced frame, as \n"
	"    <parameter>min voiced to unvoiced coeff threshold</parameter>. Must be in the \n"
	"    range zero to <parameter>unvoiced to voiced coeff threshold</parameter> \n"
	"    inclusive. Default is 0.75.\n\n"

	"-R <float> voiced to unvoiced coeff threshold-ratio  \n"
	"    set the scaling factor used in determining the correlation\n"
	"    coefficient threshold which must not be exceeded in a voiced \n"
	"    frame to unvoiced frame transition, as <parameter>voiced to unvoiced</parameter> \n"
	"    coeff threshold -ratio. The voiced to unvoiced coefficient \n"
	"    threshold is determined by multiplying this scaling factor \n"
	"    with the maximum cross-correlation coefficient of the \n"
	"    previously voiced frame. If this product is less than \n"
	"    <parameter>min voiced to unvoiced coeff threshold</parameter> then this is used \n"
	"    instead. Must be in the range zero to one inclusive. \n"
	"     Default is 0.85.\n\n"

	"-t <float> anti pitch doubling/halving threshold\n"
	"    set the threshold used in eliminating (as far as possible) \n"
	"    pitch doubling and pitch halving errors as <parameter>anti pitch \n"
	"    double/halving threshold</parameter>. Must be in the range zero to \n"
	"    one inclusive. Default is 0.77.\n\n";
}


