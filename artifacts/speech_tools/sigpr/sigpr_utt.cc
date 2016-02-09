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
/*                 Authors: Paul Taylor and Simon King                   */
/*                 Date   :  March 1998                                  */
/*-----------------------------------------------------------------------*/
/*    Signal processing functions which operate on entire utterances     */
/*                                                                       */
/*=======================================================================*/


#include "EST_error.h"
#include "EST_track_aux.h"
#include "EST_inline_utils.h"
#include "sigpr/EST_fft.h"
#include "sigpr/EST_sigpr_frame.h"
#include "sigpr/EST_sigpr_utt.h"

#include "EST_Features.h"
#include "EST_types.h"
#include "EST_string_aux.h"

void sigpr_acc(EST_Wave &sig, EST_Track &fv, EST_Features &op, 
		const EST_StrList &slist);

void sigpr_delta(EST_Wave &sig, EST_Track &fv, EST_Features &op, 
		 const EST_StrList &slist);



static void parse_op_settings(EST_Features &op, EST_WindowFunc *&wf, float &f)
{
    EST_String w_name;

    if (op.present("window_type"))
	w_name = op.S("window_type");
    else
	w_name = DEFAULT_WINDOW_NAME;
    wf = EST_Window::creator(w_name);

    f = op.present("frame_factor") ? op.F("frame_factor")
	: DEFAULT_FRAME_FACTOR;
}

void add_channels_to_map(EST_StrList &map, EST_StrList &types, 
			 EST_Features &op, int delta_order)
{
    EST_String t;
    EST_String dos;

    if (delta_order == 0)
	dos = "";
    else if (delta_order == 1)
	dos = "_d";
    else if (delta_order == 2)
	dos = "_a";
    else
	EST_error("Requested delta order too high: %d\n", delta_order);
    
 

    for (EST_Litem *s = types.head(); s; s = s->next())
    {
      t = types(s);
      if (op.present(t + "_order"))
	{
	  int actual_order = op.I(t + "_order");
	  if(actual_order < 1)
	      {
		cerr << "Invalid " << t << "_order" << " : ";
		cerr << actual_order;
		cerr << " (using 1 instead) " << endl;
		actual_order = 1;
	      }

	  int lowest_coef=0,highest_coef=actual_order-1;

	  if(t == "lpc")
	    // For lpc coefficients, we ALWAYS include energy as the
	    // 0th coefficient, so when the users gives lpc_order of
	    // 16, we produce 17 coefficients (0 to 16)
	    highest_coef=actual_order;


	  if(t == "melcep")
	    {
	      // Mel cepstra have special names - if we are not
	      // including c0, then the coefficients are numbered
	      // 1...order, and NOT 0...order-1
	      highest_coef=actual_order;
	      if(op.present("include_c0"))
		lowest_coef = 0;
	      else
		lowest_coef = 1;
	    }

	  if(actual_order == 1)
	    map.append(t + dos);
	  else
	    map.append("$" + t + dos + "-"+itoString(lowest_coef)+"+"+itoString(highest_coef));
	}
      else
	map.append(t + dos);
    }
}

void sigpr_base(EST_Wave &sig, EST_Track &fv, EST_Features &op, 
		const EST_StrList &slist)
{
    EST_Track fill, tmp;
    EST_String b_name;
    EST_String k;
    float frame_factor;
    EST_WindowFunc *wf;

    int fbank_order;
    float liftering_parameter=0;
    bool use_power_rather_than_energy=false, take_logs=true, include_c0=false;

    parse_op_settings(op, wf, frame_factor);

    for (EST_Litem *s = slist.head(); s; s = s->next())
    {
	k = slist(s);

	EST_String start_channel="0";
	if( (slist(s) == "melcep") && !op.present("include_c0"))
	  start_channel = "1";

	if (fv.has_channel(k))
	    fv.sub_track(fill, 0, EST_ALL, k , 1);
	else
	    fv.sub_track(fill, 0, EST_ALL, k + "_" + start_channel,  k + "_N");

	if(op.present("usepower"))
	    cerr << "USING POWER" << endl;

	if ((slist(s) == "lpc") || (slist(s) == "cep") 
	    ||(slist(s) == "ref") || (slist(s) == "lsf"))
	    sig2coef(sig, fill, slist(s), frame_factor, wf);
	else if (slist(s) == "power")
	    power(sig, fill, frame_factor);
	else if (slist(s) == "energy")
	    energy(sig, fill, frame_factor);
	else if (slist(s) == "f0")
	{
	    op.set("srpd_resize", 0);
	    op.set("pda_frame_shift", op.F("frame_shift"));
	    pda(sig, fill, op, "srpd");
	}
//	else if (slist(s) == "rasta")
//	  rasta(sig, fill, op);

 	else if (slist(s) == "fbank")
	{
	    use_power_rather_than_energy = op.present("usepower");
	    fbank(sig, fill, frame_factor, wf, use_power_rather_than_energy, 
		  take_logs);
	}
	
        else if (slist(s) == "melcep")
	{
	    fbank_order=op.I("fbank_order");
	    use_power_rather_than_energy = op.present("usepower");
	    include_c0=op.present("include_c0");

	    if(op.present("lifter"))
		liftering_parameter=op.F("lifter");

	    //cerr << "calling melcep " << fill.num_channels() << endl;

	    melcep(sig, fill, frame_factor, fbank_order,
		   liftering_parameter, wf, include_c0, 
		   use_power_rather_than_energy);
	}
	else
	    EST_error("Error: Unnknown type of processing requested: %s\n", 
		      ((const char*) slist(s)));
    }
}

void sigpr_delta(EST_Wave &sig, EST_Track &fv, EST_Features &op, 
		const EST_String &k)
{
    EST_Track base, fill;

//    cout << "type: " << k << endl;

    // look to see if base coefficients already exist
    EST_String start_channel="0";
    if( (k == "melcep") && !op.present("include_c0"))
      start_channel = "1";

    if (fv.has_channel(k))
	fv.sub_track(base, 0, EST_ALL, k , 1);
    else if (fv.has_channel(k + "_" + start_channel))
	fv.sub_track(base, 0, EST_ALL, k + "_" + start_channel,  k + "_N");
    else // otherwise make them in temporary track
    {
//	cout << "making tmp cpoefs\n";
	EST_StrList tmp_base, tmp_map;
	tmp_base.append(k);
	add_channels_to_map(tmp_map, tmp_base, op, 0);
	base.resize(fv.num_frames(), tmp_map);

	base.fill_time(fv);

	base.set_equal_space(false);
	sigpr_base(sig, base, op, tmp_base);
//	    cout << "BASE\n" <<  base;
//	    cout <<"after\n";
    }

    if (fv.has_channel(k + "_d"))
	fv.sub_track(fill, 0, EST_ALL, k+"_d", 1);
    else
	fv.sub_track(fill, 0, EST_ALL, k+"_d_" + start_channel,  k+"_d_N");

/*	cout << "base\n";
	track_info(base);
	cout << "fill\n";
	track_info(fill);
*/

    delta(base, fill);
}

void sigpr_acc(EST_Wave &sig, EST_Track &fv, EST_Features &op, 
	       const EST_String &k)
{
    EST_Track base, fill;

//    cout << endl << endl << "acc\n";

//    cout << "type: " << k << endl;

    // look to see if delta coefficients already exist
    EST_String start_channel="0";
    if( (k == "melcep") && !op.present("include_c0"))
      start_channel = "1";
    if (fv.has_channel(k+"_d"))
	fv.sub_track(base, 0, EST_ALL, k + "_d", 1);
    else if (fv.has_channel(k + "_d_" + start_channel))
	fv.sub_track(base, 0, EST_ALL, k + "_d_" + start_channel,  k + "_d_N");
    else // otherwise make them in temporary track
    {
	EST_StrList tmp_base, tmp_map;
	tmp_base.append(k);
	add_channels_to_map(tmp_map, tmp_base, op, 1);
	base.resize(fv.num_frames(), tmp_map);

	base.fill_time(fv);

	base.set_equal_space(false);
	sigpr_delta(sig, base, op, tmp_base);
    }

    if (fv.has_channel(k + "_a"))
	fv.sub_track(fill, 0, EST_ALL, k+"_a", 1);
    else
	fv.sub_track(fill, 0, EST_ALL, k+"_a_" + start_channel,  k+"_a_N");

//    cout << "base\n";
//    track_info(base);
//    cout << "fill\n";
//    track_info(fill);

    delta(base, fill);
}

void sigpr_acc(EST_Wave &sig, EST_Track &fv, EST_Features &op, 
		const EST_StrList &slist)
{
    for (EST_Litem *s = slist.head(); s; s = s->next())
	sigpr_acc(sig, fv, op, slist(s));
}

void sigpr_delta(EST_Wave &sig, EST_Track &fv, EST_Features &op, 
		const EST_StrList &slist)
{
    for (EST_Litem *s = slist.head(); s; s = s->next())
	sigpr_delta(sig, fv, op, slist(s));
}


int get_frame_size(EST_Track &pms, 
			 int i, int sample_rate, int prefer_prev)
{
    int prev = -1;
    int next = -1;
    
    if (i>0)
	prev = irint((pms.t(i) - pms.t(i-1))*sample_rate);
    if (i<pms.num_frames()-1)
	next = irint((pms.t(i+1) - pms.t(i))*sample_rate);
    
    if (prefer_prev)
	return prev>=0?prev:(next>=0?next:0);
    return next>=0?next:(prev>=0?prev:0);
}

float get_time_frame_size(EST_Track &pms, int i, int prefer_prev)
{
    float prev = -1;
    float next = -1;
    
    if (i > 0)
	prev = pms.t(i) - pms.t(i-1);
    if (i < pms.num_frames() -1)
	next = pms.t(i+1) - pms.t(i);
    
    if (prefer_prev)
	return prev>=0 ? prev: (next>=0 ? next : 0.0);
    return next>=0 ? next: (prev>=0 ? prev : 0.0);
}

/*void sig2lpc(EST_Wave &sig, EST_Track &lpc, EST_WindowFunc *wf, float factor)
{
    int order = lpc.num_channels() - 1;
    EST_FVector coefs(order + 1);
    int k;
    int window_start, window_size, length; // can be merged with window_size

    int sample_rate = sig.sample_rate();

    EST_FVector frame;
    
    for (k = 0; k < lpc.num_frames(); ++k)
    {
	int pos = irint(lpc.t(k) * sample_rate);
	
	length = get_local_frame_size(lpc, k, sig.sample_rate());
	window_size = irint(length * factor);
	window_start = pos - (window_size/2);

	EST_Window::window_signal(sig, wf, window_start, 
				  window_size, frame, 1);

	lpc.frame(coefs, k);
	sig2lpc(frame, coefs);
    }
    lpc.save("test.est", "est");
}   
*/

/*typedef void EST_FrameFunc(const EST_FVector &in_frame, 
			   EST_FVector &out_frame);

void sig2coef(EST_Wave &sig, EST_Track &lpc, EST_WindowFunc *wf, 
	      EST_FrameFunc *ff, float factor)
{
    EST_FVector coefs, frame;
    int start, size;
    
    for (int k = 0; k < lpc.num_frames(); ++k)
    {
	size = irint(get_local_frame_size(lpc, k, sig.sample_rate())* factor);
	start = (irint(lpc.t(k) * sig.sample_rate()) - (size/2));

	EST_Window::window_signal(sig, wf, start, size, frame, 1);

	lpc.frame(coefs, k);
	(*ff)(frame, coefs);
    }
} 
*/  

void sig2coef(EST_Wave &sig, EST_Track &tr, EST_String type, 
	      float factor, EST_WindowFunc *wf)
{
    EST_FVector coefs, frame;
    int start, size;

//    cout << "TYPE IS " << type << endl;

    for (int k = 0; k < tr.num_frames(); ++k)
    {
	if (factor < 0)  // want fixed frame rate 
	    size = (int)(-1.0 * factor * (float)sig.sample_rate());
	else
	    size = irint(get_frame_size(tr, k, sig.sample_rate())* factor);
	start = (irint(tr.t(k) * sig.sample_rate()) - (size/2));

	EST_Window::window_signal(sig, wf, start, size, frame, 1);

	tr.frame(coefs, k);
	frame_convert(frame, "sig", coefs, type);
    }
}   

void power(EST_Wave &sig, EST_Track &pow, float factor)
{
    EST_FVector frame;
    int window_start, window_size, pos, k;

    EST_WindowFunc *wf =  EST_Window::creator("rectangular");

    for (k = 0; k < pow.num_frames(); ++k)
    {
	pos = irint(pow.t(k) * sig.sample_rate());
	if (factor < 0)  // want fixed frame rate 
	    window_size = (int)(-1.0 * factor * (float)sig.sample_rate());
	else
	    window_size = irint(get_frame_size(pow, k, sig.sample_rate())
				* factor);
	window_start = pos - window_size/2;
	EST_Window::window_signal(sig, wf, window_start, window_size,frame, 1);

	sig2pow(frame, pow.a(k));
    }
}

void energy(EST_Wave &sig, EST_Track &pow, float factor)
{
    EST_FVector frame;
    int window_start, window_size, pos, k;

    EST_WindowFunc *wf =  EST_Window::creator("rectangular");

    for (k = 0; k < pow.num_frames(); ++k)
    {
	pos = irint(pow.t(k) * sig.sample_rate());
	if (factor < 0)  // want fixed frame rate 
	    window_size = (int)(-1.0 * factor * (float)sig.sample_rate());
	else
	    window_size = irint(get_frame_size(pow, k, sig.sample_rate())
				* factor);
	window_start = pos - window_size/2;
	EST_Window::window_signal(sig, wf, window_start, window_size,frame,1);

	sig2rms(frame, pow.a(k));
    }
}

static EST_String determine_type(const EST_String &intype)
{
    return (intype.contains("_") ? intype.before("_"): intype);
}

void convert_track(EST_Track &in_track, EST_Track &out_track, 
		   const EST_String &out_type, const EST_String &in_type)
{
    if (in_track.num_frames() != out_track.num_frames())
	EST_error("In track has %d frames, out track has %d\n", 
		  in_track.num_frames(), out_track.num_frames());

    EST_String tmp;
    tmp = ((in_type == "") ? determine_type(in_track.channel_name(0)):in_type);

    EST_FVector in_frame(in_track.num_channels());
    EST_FVector out_frame(out_track.num_channels());

    for (int i = 0; i < in_track.num_frames(); ++i)
    {
	in_track.frame(in_frame, i);
	out_track.frame(out_frame, i);
	frame_convert(in_frame, tmp, out_frame, out_type);
    }
}



void fbank(EST_Wave &sig,
	   EST_Track &fbank_track,
	   const float factor,
	   EST_WindowFunc *wf,
	   const bool use_power_rather_than_energy,
	   const bool take_log)
{

    // still to add : high/low pass filtering

    int window_start, window_size, pos, k;
    EST_FVector frame,fbank_frame;

    // get_order(...) gives wrong answer ... Paul ?
    int fbank_order = fbank_track.num_channels();

    // sanity check
    if(fbank_order < 1)
    {
	EST_error("Filterbank order of %i makes no sense.\n",fbank_order);
	return;
    }

    for (k = 0; k < fbank_track.num_frames(); ++k)
    {
	if (factor < 0)  // want fixed frame rate 
	    window_size = (int)(-1.0 * factor * (float)sig.sample_rate());
	else
	    window_size = irint(get_frame_size(fbank_track, k, sig.sample_rate())
				* factor);
	pos = irint(fbank_track.t(k) * sig.sample_rate());
	window_start = pos - window_size/2;
	EST_Window::window_signal(sig, wf, window_start, window_size,frame, 1);

	fbank_track.frame(fbank_frame,k);
	sig2fbank(frame,fbank_frame,sig.sample_rate(),
		  use_power_rather_than_energy,take_log);

    }


}


void melcep(EST_Wave &sig, EST_Track &mfcc_track,
	    float factor,
	    int fbank_order,
	    float liftering_parameter,
	    EST_WindowFunc *wf,
	    const bool include_c0,
	    const bool use_power_rather_than_energy)
{

    EST_FVector frame,mfcc_frame,fbank_frame;
    int k;

    // first, do filterbank analysis
    // need a temporary track, with the same setup as mfcc_track
    EST_Track fbank_track;

//    cout << "MELPCEP\n"  << fbank_order << endl;
    
    fbank_track.resize(mfcc_track.num_frames(), fbank_order);
    fbank_track.fill_time(mfcc_track);
    fbank_track.set_equal_space(false);

    // temp removed by pault 24/02/99
//    make_timed_track(mfcc_track, fbank_track, "filter", fbank_order, 0);

    // 'true' makes fbank(...) take logs
    fbank(sig, fbank_track, factor, wf, use_power_rather_than_energy, true);
    
    /*
      if(include_c0)
      cerr << "melcep c0" << endl;
      else
      cerr << "melcep no c0" << endl;
    */
    for (k = 0; k < mfcc_track.num_frames(); ++k)
    {

	mfcc_track.frame(mfcc_frame,k);
	fbank_track.frame(fbank_frame,k);

	fbank2melcep(fbank_frame, mfcc_frame,liftering_parameter,include_c0);
    }
}
