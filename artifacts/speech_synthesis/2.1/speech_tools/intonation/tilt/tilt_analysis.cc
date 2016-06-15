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
/*                    Date   :  March 1998                               */
/*-----------------------------------------------------------------------*/
/*                        Tilt Analysis                                  */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include "EST_math.h"
#include "EST_tilt.h"
#include "tilt.h"
#include "EST_Track.h"
#include "EST_track_aux.h"
#include "EST_Features.h"
#include "EST_error.h"

static int match_rf_point(EST_Track &fz, int b_start, int b_stop, 
			  int e_start, int e_stop, 
			  int &mi, int &mj);

static void make_int_item(EST_Item &e, const EST_String name, float end, 
				     float start_pos,
				     float start_f0, 
				     float peak_pos, 
				     float peak_f0);

//void rfc_segment_features_only(EST_Relation &ev);
//void convert_to_event(EST_Relation &ev);

static int rf_match(EST_Track &fz, EST_Item &ev, float range);

static int zero_cross(EST_Track &fz);

static int comp_extract(EST_Track &fz, EST_Track &part, float &start, float
			&end, float min);
//void segment_to_event(EST_Relation &ev);
//void event_to_segment(EST_Relation &ev, float min_length = 0.01);

void int_segment_to_unit(EST_Relation &a, EST_Relation &ev);

static void convert_to_local(EST_Relation &ev);

static void silence_f0(EST_Relation &ev, EST_Track &fz);

static void add_phrases(EST_Relation &ev);


// find event portions of fz in contour, cut out, and send one by one
// to individual labeller.

// This routine takes an Fz contour and a list of potential events,
// and peforms RFC matching on them. It returns a list of events with RFC
// parameters marked.
// 
// The algorithm works as follows:
// 
// make a list of events, with start and stop times.
// 
// for every event
// {
//    find start and stop times.
//    call comp_extract() to get best section of contour that 
//         falls between these times. If no suitable contour is found the
//         event is deleted and not labelled.
//    call rf_match to determine the optimal start and end times for
//           that section
// }
//
// Now add connections between non-overlapping events. Overlapping events
// get readjusted to make them simply adjacent

void default_rfc_params(EST_Features &op)
{
    op.set("start_limit", 0.1);
    op.set("stop_limit", 0.1);
    op.set("range", 0.3);
    op.set("min_event_duration", 0.03);
}
void print_event(EST_Item &ev);

void rfc_analysis(EST_Track &fz, EST_Relation &ev, EST_Features &op)
{
    EST_Item *e, *tmp, *n;
    float start_search, end_search;
    EST_Track part;
    EST_Relation eva;

    if (op.present("debug"))
    {
	cout << "rfc_recognise\n";
	cout << ev;
    }

    int_segment_to_unit(ev, eva);

    if (op.present("debug"))
    {
	cout << "rfc_recognise\n";
	cout << ev;
    }

    // fill values in event labels using matching algorithms
    for (e = ev.head(); e != 0; e = n)
    {
	n = e->next();
	// cout << endl << endl;
	if (!event_item(*e))
	    continue;
	end_search = e->F("end") + op.F("stop_limit");
	start_search = e->F("start") - op.F("start_limit");

	if (op.present("debug"))
	{
	    cout << "start = " << e->F("start") << " End " 
		 << e->F("end")<< endl;
	    cout << "s start = " << start_search << " sEnd " 
		 << end_search << endl;
	    cout << *e << endl;;
	}
	
	if (comp_extract(fz, part, start_search, end_search, 
			 op.F("min_event_duration")))
	    rf_match(part, *e, op.F("range"));
	else
	    ev.remove_item(e);
    }
    
    // hack to deal with cases where no events exist 
    if (ev.head() == 0)
    {
	tmp = ev.append();
	make_int_item(*tmp, "sil",  fz.t(0), fz.t(fz.num_frames() - 1), 
			    0.0, 0.0, 0.0);
    }

    silence_f0(ev, fz);
    add_phrases(ev);

    // cout << endl << endl << ev;
    convert_to_local(ev);

    // make sure events don't overlap
//    adjust_overlaps(ev);

    ev.f.set("intonation_style", "rfc");

    if (op.present("debug"))
    {
	cout << "After RFC analysis\n";
	cout << ev;
    }
}

// convert intonation stream from segment type to event type description.
// Note this has to be done in 2 loops.

// Create a section of fz contour, bounded by times "start" and "end".
// The created contour is defined to be the largest single continuous
// section of contour bounded by the two times. If no fz contour exits within
// the limits an error is returned.

static void convert_to_local(EST_Item *e)
{
    if (e->S("rfc.type", "0") == "RISEFALL")
    {
	e->set("rfc.rise_amp", (e->F("rfc.peak_f0") - e->F("ev.start_f0")));
	e->set("rfc.rise_dur", (e->F("rfc.peak_pos") - e->F("start")));
	e->set("rfc.fall_amp", (e->F("rfc.end_f0") - e->F("rfc.peak_f0")));
	e->set("rfc.fall_dur", (e->F("end") - e->F("rfc.peak_pos")));
	e->set("ev.f0", e->F("rfc.peak_f0"));
//	e->set("ev.f0", e->F("rfc.peak_f0") - e->F("rfc.rise_amp"));

	e->A("rfc").remove("peak_f0");
	e->A("rfc").remove("peak_pos");
	e->A("rfc").remove("end_f0");
	e->A("rfc").remove("type");
	e->A("ev").remove("start_f0");
    }
    else if (e->S("rfc.type", "0") == "RISE")
    {
	e->set("rfc.rise_amp", (e->F("rfc.end_f0") - e->F("ev.start_f0")));
	e->set("rfc.rise_dur", (e->F("end") - e->F("start")));
	e->set("rfc.fall_amp", 0.0);
	e->set("rfc.fall_dur", 0.0);
	e->set("ev.f0", e->F("rfc.end_f0"));
//	e->set("ev.f0", e->F("rfc.end_f0") - e->F("rfc.rise_amp"));
	
	e->A("rfc").remove("peak_f0");
	e->A("rfc").remove("peak_pos");
	e->A("rfc").remove("end_f0");
	e->A("rfc").remove("type");
	e->A("ev").remove("start_f0");
    }
    else if (e->S("rfc.type", "0") == "FALL")
    {
	e->set("rfc.rise_amp", 0.0);
	e->set("rfc.rise_dur", 0.0);
	e->set("rfc.fall_amp", (e->F("rfc.end_f0") - e->F("ev.start_f0")));
	e->set("rfc.fall_dur", (e->F("end") - e->F("start")));
	e->set("ev.f0", e->F("ev.start_f0"));

	e->A("rfc").remove("peak_f0");
	e->A("rfc").remove("peak_pos");
	e->A("rfc").remove("end_f0");
	e->A("rfc").remove("type");
	e->A("ev").remove("start_f0");
    }
    if (!e->f_present("time"))
	e->set("time", (e->F("end") - e->F("rfc.fall_dur")));
}

void convert_to_local(EST_Relation &ev)
{
    EST_Item *e;

    for (e = ev.head(); e; e = e->next())
	convert_to_local(e);

    // cout << "c to l \n\n\n" << ev << endl << endl;

//    ev.remove_item_feature("rfc.peak_f0");
//    ev.remove_item_feature("rfc.peak_pos");
    ev.remove_item_feature("ev.start_f0");
    ev.remove_item_feature("start");
//    ev.remove_item_feature("rfc.end_f0");
    ev.remove_item_feature("end");
//    remove_item_feature(ev, "int_event");

    ev.f.set("timing_style", "event");
}

void extract2(EST_Track &orig, float start, float end, EST_Track &ret);

static int comp_extract(EST_Track &fz, EST_Track &part, float &start, float
			&end, float min_length)
{
    int i;
    int continuous = 1;
    cout.precision(6);
    EST_Track tr_tmp, tmp2;

    if (start > end)
	EST_error("Illegal start and end times: %f %f\n", start, end);

//    int from = fz.index(start);
//    int to = fz.index_below(end);

    // cout << "full f0 = " << fz.num_frames() << endl;
//    fz.copy_sub_track(tr_tmp, from, to, 0, EST_ALL);

//    cout << "sub_track: " << tr_tmp;
    
    extract2(fz, start, end, tr_tmp);
    
//    cout << "tr_tmp 1\n" << tr_tmp;
    tr_tmp.rm_trailing_breaks();
//    cout << "tr_tmp 2\n" << tr_tmp;

//    cout << "end " << tr_tmp.end() << " start "<< tr_tmp.start() << endl;

//    i = tr_tmp.num_frames();

//    cout << "starting i = " << tr_tmp.num_frames() << endl;
//    cout << "starting i = " << tr_tmp.num_channels() << endl;
//    cout << "found end at " << i << tr_tmp.t(i) << endl;

//    cout << "tr_tmp 1\n" << tr_tmp;
    if ((tr_tmp.end() - tr_tmp.start()) < min_length)
    {
	cout << "Contour too small for analysis\n";
	return 0;
    }
    
    for (i = 0; i < tr_tmp.num_frames(); ++i)
	if (tr_tmp.track_break(i))
	    continuous = 0;
    
    // if no breaks are found in this section return. 
    if (continuous)
    {
	part = tr_tmp;
	return 1;
    }
    
    // tracks can legitimately have breaks in them due to the 
    // search region overlapping a silence. In this case we find
    // the longest single section
    // cout << "tilt_analysis: This contour has a break in it\n";
    
    int longest, s_c, s_l;
    longest = s_c = s_l = 0;
    
    for (i = 0; i < tr_tmp.num_frames(); ++i)
	if (tr_tmp.track_break(i))
	{
	    if ((i - s_c) > longest)
	    {
		longest = i - s_c - 1;
		s_l = s_c;
	    }
	    // skip to next real values
	    for (;(i < tr_tmp.num_frames()) && (tr_tmp.track_break(i)); ++i)
		s_c = i;
	}
    
    if ((i - s_c) > longest)
    {
	longest = i - s_c - 1;
	s_l = s_c;
    } 
    
    //    cout << "Longest fragment is " << longest << " starting at " << s_l <<endl;
    //    cout << "Times: " << tr_tmp.t(s_l) << " : " <<tr_tmp.t(s_l + longest) << endl;
    
    extract2(tr_tmp, tr_tmp.t(s_l), tr_tmp.t(s_l + longest), part);
//    cout << "Part\n" << part;
    part.rm_trailing_breaks();
    start = part.t(0);
    end = part.t(part.num_frames()-1);

    return 1;

}

static int zero_cross(EST_Track &fz)
{
    for (int i = 0; i < fz.num_frames() - 1; ++i)
	if ((fz.a(i) >= 0.0) && (fz.a(i + 1) < 0.0))
	    return i;
    
    return -1;
}

// 1. There should be a more sophisticated decision about whether there
// should be a risefall analysis, and if so, where the peak (zero cross)
// region should be.
// 2. There should be minimum endforced distances for rises and falls.

static int rf_match(EST_Track &fz, EST_Item &ev, float range)
{ 
    int n;
    EST_Track diff;
    int start, stop;
    int b_start, b_stop, e_start, e_stop, region;
    EST_Features empty;
    
    if (fz.num_frames() <= 0)
    {
	ev.set("start", 0.0);
	ev.set("ev", empty);
	ev.set("rfc", empty);
	ev.set("ev.start_f0", 0.0);
	ev.set("rfc.peak_f0", 0.0);
	ev.set("rfc.peak_pos", 0.0);
    }
    
    diff = differentiate(fz);

    // cout << "Extracted Contour:\n";
    //    cout << fz;
    
    n = zero_cross(diff);
    
    if (n >= 0)			// rise + fall combination
    {
	// cout << "zero crossing at point " << n << " time " << fz.t(n) << endl;
	b_start = 0;
	stop = n;
	// find rise part
	region = (int)(range * float(stop - b_start));
	// ensure region is bigger than 0
	region = region > 0 ? region : 1;
	
	b_stop = b_start + region;
	e_start = stop - region;
	e_stop = stop + region;
	// ensure regions are separate
	e_start = (e_start < b_stop)? b_stop : e_start;
	
	//printf("rise: b_start  %d, b_stop %d, end %d, end stop%d\n", b_start,
	//       b_stop, e_start, e_stop);
	match_rf_point(fz, b_start, b_stop, e_start, e_stop, start, stop);
	// cout << "Rise is at start: " << start << " Stop = " << stop << endl;
	
	ev.set("ev.start_f0", fz.a(start));
	ev.set("start", fz.t(start));
	
	// find fall part. The start of the search is FIXED by the position
	// of where the rise stopped
	
	b_start = n;
	b_stop = n + 1;
	e_stop = fz.num_frames() - 1;
	region = (int)(range * float(e_stop - b_start));
	region = region > 0 ? region : 1;
	e_start = e_stop - region;
	
	// printf("fall: b_start  %d, b_stop %d, end %d, end stop%d\n", b_start,
	//       b_stop, e_start, e_stop);
	
	match_rf_point(fz, b_start, b_stop, e_start, e_stop, start, stop);
	// cout << "Fall is at start: " << start << " Stop = " << stop << endl;
	// cout << "region: " << region << endl;
	// cout << "stop could be " << e_stop << " value " << fz.t(e_stop) << endl;
	// cout << "start could be " << e_start << " value " << fz.t(e_start) << endl;

	ev.set("rfc.peak_f0", fz.a(start));
	ev.set("rfc.peak_pos", fz.t(start));
	ev.set("rfc.end_f0", fz.a(stop));

	ev.set("end", fz.t(stop));
	ev.set("rfc.type", "RISEFALL");

/*	ev.set("rfc.setpeak_f0", fz.a(start));
 	ev.fA("rfc").set("peak_pos", fz.t(start));
	 ev.fA("rfc",1).set("end_f0", fz.a(stop));

	 ev.set("end", fz.t(stop));

	 ev.fA("rfc").set("type", "RISEFALL");
*/
	// cout << "peak pos: " << ev.F("rfc.peak_pos") << endl;
	// cout << "peak pos: " << ev.A("rfc").F("peak_pos") << endl;
	// cout << "rfc:\n" << ev.A("rfc") << endl;
	// cout << "labelled event: " << ev << endl;
    }
    else			// separate rise or fall
    {
	b_start = 0;
	e_stop = fz.num_frames() - 1;
	
	region = (int)(range * float(e_stop - b_start));
	region = region > 0 ? region : 1;
	
	b_stop = b_start + region;
	e_start = e_stop - region;
	
	// printf("b_start  %d, b_stop %d, end start %d, end stop%d\n", b_start,
	//        b_stop, e_start, e_stop);
	
	match_rf_point(fz, b_start, b_stop, e_start, e_stop, start, stop);
	
	ev.set("start", fz.t(start));
	ev.set("ev.start_f0", fz.a(start));
	ev.set("rfc.peak_f0", 0.0);
	ev.set("rfc.peak_pos", 0.0);
	
	ev.set("rfc.end_f0", fz.a(stop));
	ev.set("end", fz.t(stop));
	
	// cout  << "start " << fz.t(start) << " end " << fz.t(stop) << endl;
	
	if (fz.a(fz.index(fz.start())) < fz.a(fz.index(fz.end())))
	    ev.set("rfc.type", "RISE");
	else
	    ev.set("rfc.type", "FALL");
	
	// cout << "labelled event: " << ev << endl;
    }
    return 0;
}

static void silence_f0(EST_Relation &ev, EST_Track &fz)
{
    EST_Item * e;
    int i;

    for (e = ev.head(); e; e = e->next())
	if (sil_item(*e))
	{
	    i = fz.prev_non_break(fz.index(e->F("start")));

	    e->set("ev.start_f0", fz.a(i));
	    i = fz.next_non_break(fz.index(e->F("end")));
	    e->set("ev.end_f0", fz.a(i));
	}
}

static void add_phrases(EST_Relation &ev)
{
    EST_Item *e, *n, *s;

    // cout << "phrase edges: " << endl;
    // cout << ev;

    for (e = ev.head(); e; e = n)
    {
	n = e->next();
	if (sil_item(*e))
	{
	    if (e != ev.head())
	    {
		s = e->insert_before();
		s->set("name", "phrase_end");
		s->set("ev.f0", e->F("ev.start_f0"));
		s->set("time", e->F("start"));
	    }
	    if (e != ev.tail())
	    {
		s = e->insert_after();
		s->set("name", "phrase_start");
		s->set("ev.f0", e->F("ev.end_f0"));
		s->set("time", e->F("end"));
	    }
	}
    }

    for (e = ev.head(); e; e = n)
    {
	n = e->next();
	if (sil_item(*e))
	    ev.remove_item(e);
    }
}

/*
static void add_phrases(EST_Relation &ev, bool phrase_edges)
{
    EST_Item *e, *n, *s, *p;
    float min_duration = 0.02;

    cout << "phrase edges: " << phrase_edges << endl;
    cout << ev;

    for (e = ev.head(); next(e); e = n)
    {
	n = e->next();
	p = e->prev();
	if (!sil_item(*e))
	    continue;

	s = 0;
	
	if ((e != ev.head())  && (phrase_edges
				  || (p &&(e->F("start") - p->F("end")) 
				      > min_duration)))
	{
	    s = e->insert_before();
	    s->set("name", "phrase_end");
	    s->set("ev.f0", e->F("ev.start_f0", 1));
	    s->set("position", e->F("start"));
	}

	if (phrase_edges || (n &&((n->F("start")- e->F("end")) >min_duration)))
	{
	    s = e->insert_after();
	    s->set("name", "phrase_start");
	    s->set("ev.f0", e->F("ev.end_f0",1));
	    s->set("position", e->F("end"));
	}

	if (s == 0)
	{
	    s = e->insert_before();
	    s->set("name", "pause");
	    s->set("position", e->F("start"));
	}
    }

    s = e->insert_after();
    s->set("name", "phrase_end");
    s->set("ev.f0", e->F("ev.start_f0", 1));
    s->set("position", e->F("end"));

    for (e = ev.head(); e; e = n)
    {
	n = e->next();
	if (sil_item(*e))
	    ev.remove_item(e);
    }
    cout << "end phrase edges\n";
}
*/
static void make_int_item(EST_Item &tmp, 
			  const EST_String name, float end, float start,
			  float start_f0, float peak_pos, 
			  float peak_f0)

{
    tmp.set_name(name);
    EST_Features dummy;
    
    tmp.set("start", start);
    tmp.set("end", end);
    tmp.set("ev", dummy);
    tmp.set("ev.start_f0", start_f0);
    
    if ((name != "sil") && (name != "c"))
    {
	tmp.set("rfc", dummy);
	tmp.set("rfc.peak_pos", peak_pos);
	tmp.set("rfc.peak_f0", peak_f0);
	tmp.set("rfc.pos", 1);
    }
}

static float distance(EST_Track &fz, int pos, EST_Track &new_fz, int
	       num_points)
{
    int i;
    float distance = 0.0;
    float diff;
    
    for (i = 0; i < num_points; ++i)
    {
	diff = fz.a(i + pos) - new_fz.a(i);
	/*	    dprintf("o = %f, n = %f\n", old_contour[i + pos],
		    new_contour[i]);  */
	distance += (diff * diff);
    }
    return (distance); 
}

static float weight(float duration)
{
    (void)duration;
    /*    return ((MAX_DUR + 0.7) - duration); */
    return(1.0);
}

// Return indexs in fz to best fitting region of monomial curve to
// fz contour. The search is bounded by the b/e_start an b/e_stop
// values. The contour fz, should have no breaks in it.

static int match_rf_point(EST_Track &fz, int b_start, int b_stop, 
			  int e_start, int e_stop, int &mi, int &mj)
{
    int i, j, k;
    float s_pos, e_pos, s_freq, e_freq, t;
    float amp, duration, dist, ndist;
    float min_dist = MAXFLOAT;
    int length;
    EST_Track new_fz(fz.num_frames(), 1);
    float f_shift;
    
    mi = mj = 0;		// set values to zero for safety
    
    if ((b_start >= b_stop) || (b_start < 0))
    {
	cerr << "Illegal beginning search region in match_rf_point:" <<
	    b_start << "-" << b_stop << endl;
	return -1;
    }
    if ((e_start >= e_stop) || (e_stop > fz.num_frames()))
    {
	cerr << "Illegal ending search region in match_rf_point:" <<
	    e_start << "-" << e_stop << endl;
	return -1;
    }
    
    f_shift = fz.shift();
    duration = 0.0;
    
    for (i = b_start; i < b_stop; ++i)
	for (j = e_start; j < e_stop; ++j)
	{
	    s_pos = fz.t(i);
	    s_freq = fz.a(i);
	    
	    e_pos = fz.t(j);
	    e_freq = fz.a(j);
	    
	    duration = e_pos - s_pos;
	    amp = e_freq - s_freq;
	    length = j - i;
	    
	    for (k = 0; k < length + 1; ++k)
	    {
		t = ((float) k) * f_shift;
		new_fz.a(k) = (amp * fncurve(duration, t, 2.0)) 
		    + s_freq;
	    }
	    
	    dist = distance(fz, i, new_fz, length);
	    ndist = dist / (duration * 100.0);
	    ndist *= weight(duration);
	    
	    if (ndist < min_dist)
	    {
		min_dist = ndist;
		mi = i;
		mj = j;
	    }
	}
    return 0;
}

/*
#if 0

static void fill_f0_values(EST_Track &fz, EST_Relation &ev)
{
    EST_Item *e;
    float start_a;
    int pos;
    float prev = 0.0;
    
    for (e = ev.head(); e != 0; e = e->next())
    {
	if (e->name() == "sil")
	{
	    pos =  fz.index(prev);
	    pos = fz.prev_non_break(pos);
	    start_a = pos > 0 ? fz.a(pos) : 0.0;
	}
	else if (e->name() == "c")
	{
	    pos =  fz.index(prev);;
	    pos = fz.next_non_break(pos);
	    start_a = fz.a(pos);
	}
	else 
	    start_a = fz.a(prev);
	
	e->set("ev:start_f0", start_a);
	e->set("start", prev);
	//	cout << "setting start to be " << start_a << " at pos " << pos << endl;
	//	cout << *e << " " << *RFCS(*e) << endl;
	
	if (e->f("rfc:type") == "RISEFALL")
	{
	    start_a = fz.a(e->F("rfc:peak_pos"));
	    e->set("rfc:peak_f0", start_a);
	}
	prev = e->f("end");
    }
}

static void insert_silence(EST_Item *n, EST_Track &fz, int i, int j)
{
    EST_Item *prev_item, *new_sil;
    float min_length = 0.015;
    float sil_start, sil_end, start_f0;
    
    sil_start = i > 0 ? fz.t(i - 1) : 0.0;
    sil_end = fz.t(j);
    
    if ((sil_end - sil_start) < min_length)
	return;
    
    // add silence
    start_f0 = (i > 0) ? fz.a(i -1) : fz.a(i);
    new_sil = n->insert_after();
    make_int_item(*new_sil, "sil", sil_end, sil_start, start_f0, 0.0, 0.0);
    new_sil->set("rfc:type", "SIL");
    
    // if silence leaves a gap, make a new element before it
    if ((sil_start - n->F("start")) < min_length)
	return;
    
    // make new element, setting end time to silence start time.
    prev_item = n->prev()->insert_before();
    make_int_item(*prev_item, n->name(), sil_start, 0.0, n->f("ev:start_f0"),
			      0.0,0.0);

    // now tidy up values of existing element
    n->set("ev:start_f0", fz.a(j));
    n->set("start", sil_end);
}

static void add_phrases_old(EST_Relation &ev, EST_Track &fz)
{
    int i;
    EST_Item *e, *n, *s;
    bool sil = false;
    float start, end;

    for (e = ev.head(); next(e); e = n)
    {
	n = e->next();
	start = e->F("end");
	end = n->F("start");
	sil = false;

	cout << endl << endl;

	cout << *e << endl;
	cout << *n << endl;

	cout << "start = " << start << endl;
	cout << "end = " << end << endl;
	
	for (i = fz.index(start); i < fz.index(end); ++i)
	{
	    cout << i << endl;
	    cout << fz.val(i) << endl;
	    if ((!sil) &&(!fz.val(i)))
	    {
		cout << "phrase_end\n";
		sil = true;
		s = e->insert_after();
		s->set("name", "phrase_end");
		s->set("position", fz.t(i - 1));
		if (i > (fz.index(start) + 1))
		    s->set("ev:f0", fz.a(i - 1));
		e = s;
	    }

	    if (sil && fz.val(i)) // just come out of silence
	    {
		cout << "phrase_start\n";
		sil = false;
		s = e->insert_after();
		s->set("name", "phrase_start");
		s->set("position", fz.t(i));
		s->set("ev:f0", fz.a(i));
	    }
	}
    }
}

static void add_silences(EST_Track &fz, EST_Relation &ev, 
			 float end_sil_length)
{
    int i, j;
    EST_Item *e;
    
    for (i = 0; i < fz.num_frames(); ++i)
	if (fz.track_break(i))
	{
	    for (j = i; j < fz.num_frames(); ++j)
		if (fz.val(j))
		    break;
	    if (j == fz.num_frames())	// off end of array
		break;
	    cout << "silence between " <<i << " and " << j << endl;
	    //	    cout << "  " << fz.t(i) << " and " << fz.t(j) << endl;
	    
	    for (e = ev.head(); e != 0; e = e->next())
		if (e->F("end") >= fz.t(j))
		    break;
	    insert_silence(e, fz, i, j);
	    //	    for (e = ev.head(); e != 0; e = e->next())
	    //		cout << *e << " : " << *RFCS(*e) << endl;
	    
	    i = j;
	}
    
    if (sil_item(*ev.tail()))
	return;
    
    float start_f0 = fz.a(fz.end());

    e = ev.append();    
    make_int_item(*e, "sil", fz.end() + end_sil_length, fz.end(), 
			    start_f0, 0.0, 0.0);
    e->set("rfc:type", "SIL");
}
*/
/*static void minimum_duration(EST_Relation &ev, float min_dur)
{
    EST_Item *e, *n;
    
    for (e = ev.head(); e != 0; e = n)
    {
	n = e->next();
	if (dur(*e) < min_dur)
	    ev.remove_item(e);
    }
}

static void adjust_overlaps(EST_Relation &ev)
{
    EST_Item *e, *n;
    float pos=0.0;
    
    for (e = ev.head(); next(e) != 0; e = e->next())
    {
	n = e->next();
	if (e->F("end") > n->F("start"))
	{
*/
/*	    cout << "Overlapping events " << *e <<":" << *n << endl;
	    // case a: genuine overlap
	    if (n->F("end") > e->F("end"))
	    {
		cout << "case A\n";
//		pos = (e->F("end") + start(n)) / 2.0;
	    }
	    
	    // case b: second element is enclosed by first
	    else if (n->F("end") <= e->F("end"))
	    {
		cout << "case A\n";
//		pos = start(n);
	    }
	    
	    // case c: second element is before first
*	    else if ((n->F("end") < e->F("end")) &&
		     (start(n) < start(e)))
	    {
		cout << "case A\n";
		pos = (n->F("end") + start(e)) / 2.0;
	    }
	    else
		cout << "No overlap conditions met\n";
	    //	    cout << "pos =" << pos << endl;
*/
/*	    e->set("end", pos);
	    n->set("start", pos);
	    cout << endl << endl;
	}
    }
    
    // The overlap adjustments may cause the peak position to lie outside
    // the start and end points. This checks for this and makes an
    // arbitrary adjustment
    for (e = ev.head(); next(e) != 0; e = e->next())
	if ((e->f("rfc:type") == "RISEFALL") && (e->F("rfc:peak_pos") <
						 e->F("start")))
	    e->set("rfc:peak_pos", 
		    (e->F("start") + e->F("end") / 2.0));
}

static void conn_ends(EST_Track &fz, EST_Relation &ev, float min_length)
{
    EST_Item *e, *n, *tmp;
    float t, f0;
    const float ARB_DISTANCE = 0.1;
    
    cout << min_length << endl;
    
    for (e = ev.head(); next(e) != 0; e = e->next())
    {
	n = e->next();
	cout << *e << ":";
	cout << "n: " << n->F("start") << " e "<< e->F("end") << endl;
	
	if ((n->F("start") - e->F("end")) > min_length)
	{
	    cout << "making connection\n";
	    tmp = n->insert_before();
	    make_int_item(*tmp, "c", n->f("start"), e->f("start"),
				e->f("rfc:end_f0"), 0.0, 0.0);

	    e = e->next();		// advance after new connection
	} 
	else
	{ 
	    t = (n->F("start") + e->F("end")) /2.0;
	    n->set("start", t);
	    e->set("end", t);
	}
    }
    
    t = (ev.head())->f("start"); // store time of start of first event

    // insert silence at beginning if contour doesn't start at near time 0
    //    if (fz.start() > fz.shift())
    //    {
    //	tmp = make_int_item("sil", fz.start(), 0.0, 0.0, 0.0, 0.0);
    //	ev.prepend(tmp);
    //    }
    // add connection between silence and first event

    tmp = ev.head()->insert_after();
    make_int_item(*tmp, "c", t, 0.0, fz.a(fz.start()), 0.0, 0.0);
    
    if ((ev.tail()->F("end") + min_length) < fz.end())
    {
	f0 = fz.a(ev.tail()->F("end"));
	// add connection after last event.
	//ev.insert_after(ev.tail(), tmp);
	tmp = ev.append();
	make_int_item(*tmp, "c", fz.end(), 0.0, f0, 0.0, 0.0);
    }
    
    // add silence, an arbitrary distance after end - what a hack!
    //    ev.insert_after(ev.tail(), tmp);
    tmp = ev.append();
    make_int_item(*tmp, "sil", fz.end() + ARB_DISTANCE, 
			0.0, fz.a(fz.end()), 0.0, 0.0);
}
*/



