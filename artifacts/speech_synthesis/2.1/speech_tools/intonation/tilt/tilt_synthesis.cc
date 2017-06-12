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
/*                    Date   :  February 1996 - August 98                */
/*		      RFC Synthesis                                      */
/*                                                                       */
/*=======================================================================*/

#include "tilt.h"
#include "EST_unix.h"
#include "EST_math.h"
#include "EST_tilt.h"
#include "EST_Track.h"
#include "EST_error.h"


void tilt_synthesis(EST_Track &fz, EST_Relation &ev, float f_shift, 
		    int no_conn)
{
    tilt_to_rfc(ev);

    rfc_synthesis(fz, ev, f_shift, no_conn);

    ev.remove_item_feature("rfc");
}

void synthesize_rf_event(EST_Track &fz, EST_Features &ev, float peak_f0)
{
    float t, amp, f_shift, a=0, start_f0;
    float dur=0;  // for egcs
    int j;

    f_shift = fz.shift();

    dur  = ev.F("rise_dur");
    amp = ev.F("rise_amp");
    start_f0 = peak_f0 - amp;

    for (j = 0, t = 0.0; t < dur; t += f_shift, ++j)
    {
	a = unit_curve(amp, dur, t) + start_f0;
	if (a > fz.a(j)) // overlap check
	    fz.a(j) = a;
	fz.set_value(j);
    }

    dur  = ev.F("fall_dur");
    amp = ev.F("fall_amp");

    for (t = 0.0; t < dur; t += f_shift, ++j)
    {
	a = unit_curve(amp, dur, t) + peak_f0;
	if (a > fz.a(j)) // overlap check
	    fz.a(j) = a;
	fz.set_value(j);
    }
    // hack to fill final values because of timing rounding errors
    for (; j < fz.num_frames(); ++j)
	fz.a(j) = a;
}

void fill_connection_values(EST_Track &fz, float start_f0, float start_pos,
		 float end_f0, float end_pos)
{
    float f_shift, m;
    int j;
    f_shift = fz.shift();
    if ((end_pos - start_pos) == 0)
	m = 0.0;
    else
	m = (end_f0 - start_f0) / (end_pos - start_pos);
	for (j = 0; j < fz.num_frames()-1; ++j)
	{
		fz.a(j) = (m * (float) j * f_shift) + start_f0;
		fz.set_value(j);
	}
	fz.a(fz.num_frames()-1) = end_f0;
	fz.set_value(fz.num_frames()-1);
		// hack to fill final values because of timing rounding errors
    //a = fz.a(j -1);  // I Think this is ezafi
    //for (; j < fz.num_frames(); ++j)
	//fz.a(j) = a;
}


void rfc_synthesis(EST_Track &fz, EST_Relation &ev, float f_shift, int no_conn)
{
    EST_Item *e,*nn;
    EST_Track sub;
    float start_pos=0, start_f0=0;
    int start_index, end_index;
    float end_pos, end_f0;
    int n;

    if (event_item(*ev.tail())) 
	n = (int)(ceil((ev.tail()->F("time") + 
			ev.tail()->F("rfc.fall_dur",0)) / f_shift)) + 1;
    else
	n = (int)(ceil(ev.tail()->F("time")/ f_shift)) + 1;

    fz.resize(n, 1);
    fz.set_equal_space(true);
    fz.fill(0.0);
    fz.fill_time(f_shift);

    // set default to be break (silence)
    for (int i = 0; i < fz.num_frames(); ++i)
	fz.set_break(i);

    // synthesize events
    for (e = ev.head(); e != 0; e = e->next())
    {
	if (event_item(*e))
	{
	    start_pos = e->F("time") - e->F("rfc.rise_dur");
	    end_pos = e->F("time") + e->F("rfc.fall_dur");

	   	if ((start_pos / f_shift-(int)start_pos / f_shift)>.5)
			start_index=int(start_pos / f_shift+1);
		else
			start_index = (int) start_pos / f_shift;	
		if(end_pos / f_shift-(int)end_pos / f_shift>.5)
			end_index = int( end_pos / f_shift+1); 
		else
			end_index = (int) end_pos / f_shift; 
//	    cout << "a: " << fz.equal_space() << endl;

	    fz.sub_track(sub, start_index, (end_index - start_index) + 1, 
			 0, EST_ALL);

//	    cout << "a: " << fz.equal_space() << endl;
//	    cout << "b: " << sub.equal_space() << endl;

	    synthesize_rf_event(sub, e->A("rfc"), e->F("ev.f0"));
	}
    }

    if (no_conn)
	return;

    // synthesize connections
    for (e = ev.head(); e->next() != 0; e = e->next())
    {
	if (e->S("name") == "phrase_end")
	    continue;
	nn = e->next();

	// calculate start and stop times, with additional
	// optional adjustment for rise and falls on events

	start_f0 = e->F("ev.f0") + e->F("rfc.fall_amp", 0.0);
	start_pos= e->F("time") + e->F("rfc.fall_dur", 0.0);

	end_f0 = nn->F("ev.f0") - nn->F("rfc.rise_amp", 0.0);
	end_pos = nn->F("time") - nn->F("rfc.rise_dur", 0.0);

	if ((start_pos / f_shift-(int)start_pos / f_shift)>.5)
		start_index=int(start_pos / f_shift+1);
	else
		start_index = (int) start_pos / f_shift; 
	if(end_pos / f_shift-(int)end_pos / f_shift>.5)
		end_index = int( end_pos / f_shift+1); 
	else
		end_index = (int) end_pos / f_shift; 


	if (start_index >= end_index) // no connection needed
	    continue;

	fz.sub_track(sub, start_index, end_index - start_index+1 , 0, EST_ALL); 

	fill_connection_values(sub, start_f0, start_pos, end_f0, end_pos);
    }
}

/*

// find event portions of fz in contour, cut out, and send one by one
// to individual labeller.
void fill_rise_fall_values(EST_Track &fz, float amp, float dur, float
		start_f0, float start_pos, float f_shift, EST_String type, int nframes)
{
    float t, a;

    // this ensures rounding errors don't multiply
    int j = (int) rint(start_pos / f_shift); 
    int n = 0;
    
//    for (t = 0.0; t < (dur + (f_shift /2.0)); t += f_shift, ++j, ++n)
    for (t = 0.0; n < nframes; t += f_shift, ++j, ++n)
    {
	a = unit_curve(type, amp, dur, t) + start_f0;
	if (a > fz.a(j)) // overlap check
	    fz.a(j) = a;
	fz.set_value(j);
    }
    cout << "curve frames: " << n << endl;
}

void fill_connection_values(EST_Track &fz, float start_f0, float start_pos,
		 float end_f0, float end_pos,
		 float f_shift)
{
    // this ensures rounding errors don't multiply
    int j = (int) rint(start_pos / f_shift); 

    float m = (end_f0 - start_f0) / (end_pos - start_pos);

    if (!finite(m))
	m = 0.0;

    int pos = fz.index(start_pos);

    for (j = pos; j < (fz.index(end_pos) + 1); ++j)
    {
	fz.a(j) = (m * (float) (j -pos) * f_shift) + start_f0;
	fz.set_value(j);
    }
}


void fill_rise_fall_values(EST_Track &fz, float amp, float start_f0)
{
    float t, a;
    int j;
    float f_shift = fz.shift();
    float dur = fz.num_frames() * f_shift;
    
    for (j = 0, t = 0.0; j < fz.num_frames(); t += f_shift, ++j)
    {
	a = unit_curve("RISE", amp, dur, t) + start_f0;
	if (a > fz.a(j)) // overlap check
	    fz.a(j) = a;
	fz.set_value(j);
    }
}

void fill_connection_values(EST_Track &fz, float start_f0, float end_f0)
{
    // this ensures rounding errors don't multiply
    int j;
    float f_shift = fz.shift();
    float dur = fz.num_frames() * f_shift;

    float m = (end_f0 - start_f0) / dur;

    if (!finite(m))
	m = 0.0;

    for (j = 0 j < fz.num_frames(); ++j)
    {
	fz.a(j) = (m * (float)j * f_shift) + start_f0;
	fz.set_value(j);
    }

}


#if 0
void start_f0_pos(EST_Item *e, const EST_String &type, float &start_f0, 
		 float &start_pos)
{
    if (type == "RISE")
    {
	start_f0 = e->F("ev.f0");
	start_pos = e->F("position") - e->F("rfc.rise_dur");
    }
    else
    {
	start_f0 = e->F("ev.f0") + e->F("rfc.rise_amp");
	start_pos = e->F("position");
    }
}
#endif

static float find_start_pos(EST_Item *e, const EST_String &type)
{
    //cout << "find start position for " << *e << endl;
    if (type == "RISE")
	return e->F("position") - e->F("rfc.rise_dur");
    else
	return e->F("position");
}

static float find_start_f0(EST_Item *e, const EST_String &type)
{
    //cout << "find start f0 for " << *e<< endl;
    if (type == "RISE")
	return e->F("ev.f0");
    else
	return e->F("ev.f0") + e->F("rfc.rise_amp");
}

float rfc_dur(EST_Item *e)
{
    return e->F("rfc.rise_dur") + e->F("rfc.fall_dur");
}

float rfc_amp(EST_Item *e)
{
    return e->F("rfc.rise_amp") + e->F("rfc.fall_amp");
}



int rfc_synthesis_ld(EST_Track &fz, EST_Relation &ev, float f_shift, int no_conn)
{
    EST_Item *e,*nn;
    EST_Track sub;
    float start_pos=0, start_f0=0;
    EST_String type;
    (void)no_conn;
    int start_index, nframes, end_index;
    float length, end_pos;
    
    float last_time = ev.tail()->F("position") + rfc_dur(ev.tail());
    int n = (int)(2 + (last_time / f_shift));
    fz.resize(n, 1);
    fz.fill(0.0);
    fz.fill_time(f_shift);

    fill_rfc_types(ev);
    
    // set default to be break (silence)
    for (int i = 0; i < fz.num_frames(); ++i)
	fz.set_break(i);
    
    for (e = ev.head(); e != 0; e = e->next())
    {
	//	cout << "\ntype: " << e->fS("rfc.type") << endl;
	//cout << "\ntype: " << *e << endl;
	if (e->f("rfc.type",1) == "RISEFALL")
	{
	    start_f0 = find_start_f0(e,"RISE");
	    start_pos = find_start_pos(e,"RISE");

	    start_index = (int) rint(start_pos / f_shift); 
	    nframes = (int)((e->F("rfc.rise_dur")+ (f_shift /2.0))/f_shift);
	    fz.sub_track(sub, start_index, nframes, 0, EST_ALL);

	    fill_rise_fall_values(sub, e->F("rfc.rise_amp"), start_f0);
	    cout << "rise subtrack: " << sub;

	    start_index = (int) rint(find_start_pos(e, "FALL") / f_shift); 
	    nframes = (int)((e->F("rfc.fall_dur") +(f_shift /2.0))/f_shift);
	    fz.sub_track(sub, start_index, nframes, 0, EST_ALL);

	    fill_rise_fall_values(sub, e->F("rfc.fall_amp"), 
				  find_start_f0(e,"FALL"));

	    cout << "fall subtrack: " << sub;


	    fill_rise_fall_values(sub, e->F("rfc.fall_amp"), e->F("rfc.fall_dur"),
	 		find_start_f0(e,"FALL"),
		 	find_start_pos(e,"FALL"),
			f_shift, "FALL", nframes);

	    
	}
	else if (e->f("rfc.type",1) == "RISE")
	{
	    start_f0 = find_start_f0(e,"RISE");
	    start_pos = find_start_pos(e,"RISE");

	    start_index = (int) rint(start_pos / f_shift); 
	    nframes = (int)((e->F("rfc.rise_dur")+ (f_shift /2.0))/f_shift);
	    fz.sub_track(sub, start_index, nframes, 0, EST_ALL);

	    fill_rise_fall_values(sub, e->F("rfc.rise_amp"), start_f0);


	    fill_rise_fall_values(fz, e->F("rfc.rise_amp"),
				   e->F("rfc.rise_dur"), 
				  start_f0, start_pos,
				  f_shift, "RISE", nframes);


	}
	else if (e->f("rfc.type",1) == "FALL")
	{
	    start_f0 = find_start_f0(e, "FALL");
	    start_pos = find_start_pos(e, "FALL");

	    nframes = (int)((e->F("rfc.fall_dur")+ (f_shift /2.0))/f_shift);
	    start_index = (int) rint(find_start_pos(e, "FALL") / f_shift); 
	    fz.sub_track(sub, start_index, nframes, 0, EST_ALL);

	    fill_rise_fall_values(fz, e->F("rfc.fall_amp"), 
				  e->F("ev.f0"));

	    fill_rise_fall_values(fz, e->F("rfc.fall_amp"),
				  e->F("rfc.fall_dur"), e->F("ev.f0"), 
				  e->F("position"), f_shift,
				  "FALL", nframes);

	}
	else 
	{
	    EST_Item *nn,*pp;

	    if (no_conn)
		continue;

	    if (e->f("name",1) == "phrase_end")
	    {
		if (e->f_present("ev.f0"))
		{
		    pp = e->prev();

		    fill_connection_values(fz, start_f0 + rfc_amp(pp), 
					   start_pos
					   + rfc_dur(pp), e->F("ev.f0"),
					   e->F("position"), f_shift);

		}
	    }
	    else if (e->f("name", 1) == "phrase_start")
	    {
		//cout << "phrase start:\n" << *e << endl;
		if ((nn = e->next()) == 0)
		    EST_error("phrase start command occurs as last item "
			      "in rfc synthesis\n");
		else if (event_item(*nn))
		    {
			start_f0 = find_start_f0(nn,"RISE");
			start_pos = find_start_pos(nn,"RISE");
		    }
		else
		    {
			start_f0 = nn->F("ev.f0");
			start_pos = nn->F("position");
		    }

		fill_connection_values(fz, e->F("ev.f0"), 
				       e->F("position"),
				       start_f0,start_pos, f_shift);
		
	    }
	    else if (e->f("name") == "pause")
	    {}
	    else
		EST_error("Unable to synthesis intonation element %s\n", 
			  (const char *)(e->fS("name")));
	    continue;
	}
	if (((nn = e->next()) != 0) && (event_item(*nn)))
	{
	    float f0 = start_f0+rfc_amp(e);
	    float pos = start_pos + rfc_dur(e);
	    float end_f0 = find_start_f0(nn,"RISE");
	    float end_pos = find_start_pos(nn,"RISE");
	    fill_connection_values(fz, f0, pos, end_f0, end_pos, f_shift);
	}
    }
}
*/
