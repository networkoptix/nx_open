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
/*                    Date   :  February 1996                            */
/*-----------------------------------------------------------------------*/
/*                    Event RFC Utilities                                */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include "EST_math.h"
#include "tilt.h"
#include "EST_error.h"

void validate_rfc_stream(EST_Relation &ev);

/*
float rfc_to_tilt_amp(EST_Item *e)
{
    return fabs(e->F("rfc.rise_amp")) + fabs(e->F("rfc.fall_amp"));
}

float rfc_to_tilt_dur(EST_Item *e)
{
    return e->F("rfc.rise_dur") + e->F("rfc.fall_dur");
}

float rfc_to_a_tilt(EST_Item *e)
{
    return (float)(fabs(e->F("rfc.rise_amp")) - fabs(e->F("rfc.fall_amp"))) / 
	(float)(fabs(e->F("rfc.rise_amp")) + fabs(e->F("rfc.fall_amp")));
}

float rfc_to_d_tilt(EST_Item *e)
{
    return (float)(fabs(e->F("rfc.rise_dur")) - fabs(e->F("rfc.fall_dur"))) / 
	(float)(e->F("rfc.rise_dur") + e->F("rfc.fall_dur"));
}

float rfc_to_t_tilt(EST_Item *e)
{
    float t_tilt;
    t_tilt = (rfc_to_a_tilt(e) + rfc_to_d_tilt(e)) / 2;
    if (isnan(t_tilt))
	t_tilt = 0.0;
    return t_tilt;
}

float tilt_to_rise_amp(EST_Item *e)
{
    return e->F("tilt.amp") * (1 + e->F("tilt.tilt")) / 2.0;
}

float tilt_to_rise_dur(EST_Item *e)
{
    return e->F("tilt.dur") * (1 + e->F("tilt.tilt")) / 2.0;
}

float tilt_to_fall_amp(EST_Item *e)
{
    return -e->F("tilt.amp") * (1 - e->F("tilt.tilt")) / 2.0;
}

float tilt_to_fall_dur(EST_Item *e)
{
    return e->F("tilt.dur") * (1 - e->F("tilt.tilt")) / 2.0;
}

float tilt_to_peak_f0(EST_Item *e)
{
    return  e->F("ev:start_f0") + tilt_to_rise_amp(e);
}

float tilt_to_peak_pos(EST_Item *e)
{
    return  e->F("start") + tilt_to_rise_dur(e);
}
*/

float rfc_to_tilt_amp(EST_Features &e)
{
    return fabs(e.F("rise_amp")) + fabs(e.F("fall_amp"));
}

float rfc_to_tilt_dur(EST_Features &e)
{
    return e.F("rise_dur") + e.F("fall_dur");
}

float rfc_to_a_tilt(EST_Features &e)
{
    return (float)(fabs(e.F("rise_amp")) - fabs(e.F("fall_amp"))) / 
	(float)(fabs(e.F("rise_amp")) + fabs(e.F("fall_amp")));
}

float rfc_to_d_tilt(EST_Features &e)
{
    return (float)(fabs(e.F("rise_dur")) - fabs(e.F("fall_dur"))) / 
	(float)(e.F("rise_dur") + e.F("fall_dur"));
}

float rfc_to_t_tilt(EST_Features &e)
{
    float t_tilt;
    t_tilt = (rfc_to_a_tilt(e) + rfc_to_d_tilt(e)) / 2;
    if (isnanf(t_tilt))
	t_tilt = 0.0;
    return t_tilt;
}

float tilt_to_rise_amp(EST_Features &e)
{
    return e.F("amp") * (1 + e.F("tilt")) / 2.0;
}

float tilt_to_rise_dur(EST_Features &e)
{
    return e.F("dur") * (1 + e.F("tilt")) / 2.0;
}

float tilt_to_fall_amp(EST_Features &e)
{
    return -e.F("amp") * (1 - e.F("tilt")) / 2.0;
}

float tilt_to_fall_dur(EST_Features &e)
{
    return e.F("dur") * (1 - e.F("tilt")) / 2.0;
}

float tilt_to_peak_f0(EST_Item *e)
{
    return  e->F("ev:start_f0") + tilt_to_rise_amp(e->A("tilt"));
}

float tilt_to_peak_pos(EST_Item *e)
{
    return  e->F("start") + tilt_to_rise_dur(e->A("tilt"));
}


void rfc_to_tilt(EST_Features &rfc, EST_Features &tilt)
{
    tilt.set("amp", rfc_to_tilt_amp(rfc));
    tilt.set("dur", rfc_to_tilt_dur(rfc));
    tilt.set("tilt", rfc_to_t_tilt(rfc));
}

void rfc_to_tilt(EST_Relation &ev)
{
    EST_Item *e;
    EST_Features f;

    if (ev.f("intonation_style") != "rfc")
	EST_error("Can't create Tilt parameters from intonation style: %s\n", 
		  (const char *)ev.f.S("intonation_style"));
    
    for (e = ev.head(); e != 0; e = e->next())
	if (event_item(*e))
	{
	    e->set("tilt", f);
	    rfc_to_tilt(e->A("rfc"), e->A("tilt"));
	}
    ev.f.set("intonation_style", "tilt");
}

void tilt_to_rfc(EST_Features &tilt, EST_Features &rfc)
{
     rfc.set("rise_amp", tilt_to_rise_amp(tilt));
     rfc.set("rise_dur", tilt_to_rise_dur(tilt));
     rfc.set("fall_amp", tilt_to_fall_amp(tilt));
     rfc.set("fall_dur", tilt_to_fall_dur(tilt));
}


void tilt_to_rfc(EST_Relation &ev)
{
    EST_Item *e;
    EST_Features f;

    if (ev.f("intonation_style") != "tilt")
	EST_error("Can't create RFC parameters for intonation_style: %s\n", 
		  (const char *)ev.f.S("intonation_style"));
    
    for (e = ev.head(); e ; e = e->next())
	if (event_item(*e))
	{
	    e->set("rfc", f);
	    tilt_to_rfc(e->A("tilt"), e->A("rfc"));
	}

    ev.f.set("intonation_style", "rfc"); // say that this now contains rfc
}

void scale_tilt(EST_Relation &ev, float shift, float scale)
{
    EST_Item *e;

    for (e = ev.head(); e; e = e->next())
    {
	e->set("ev.f0", (e->F("ev.f0") + shift));
	if (e->f_present("int_event"))
	    e->set("tilt.amp", (e->F("tilt.amp") * scale));
    }
}

void fill_rfc_types(EST_Relation &ev)
{
    EST_Item *e;
    
    for (e = ev.head(); e; e = e->next())
    {
	if (event_item(*e))
	{
	    if ((e->F("rfc.rise_amp") > 0.0) && (e->F("rfc.fall_amp") < 0.0))
		e->set("rfc.type", "RISEFALL");
	    else if (e->F("rfc.rise_amp") > 0.0)
		e->set("rfc.type", "RISE");
	    else 
		e->set("rfc.type", "FALL");
	}
	else
	    e->set("rfc.type", "SIL");
    }
}

void int_segment_to_unit(EST_Relation &int_lab, EST_Relation &ev_lab)
{
    EST_Item *e, *inext;
    (void)ev_lab;
    float start = 0.0;

    if (int_lab.f("timing_style") != "segment")
	EST_error("Undefined timing style:%s in relation\n", 
		  (const char *)int_lab.f.S("timing_style"));

    for (e = int_lab.head(); e != 0; e = e->next())
    {
	e->set("start", start);
	start = e->F("end");
    }

    for (e = int_lab.head(); e != 0; e = inext)
    {
	inext = e->next();
	if (event_item(*e) || sil_item(*e))
	    continue;
	int_lab.remove_item(e);
    }

/*    for (e = int_lab.head(); e != 0; e = inext)
    {
	if (event_item(*e))
	    ev_lab.append(e);
    }
*/

    int_lab.f.set("timing_style", "unit");

}

void fn_start_to_real_start(EST_Relation &ev)
{
    for (EST_Item *e = ev.head(); e; e = e->next())
	e->set("start", e->F("start"));
}

void set_fn_start(EST_Relation &ev)
{
    for (EST_Item *e = ev.head(); e; e = e->next())
	e->set_function("start", "standard+start");

}

// this should move to EST_Relation_aux.cc after REORG. This
// adds a dummy stream_item with no name.
void event_to_segment(EST_Relation &ev, float min_length)
{
    EST_Item *e, *n;
    EST_Item *dummy;

    // REORG - replace by stream feature
    if (ev.f.S("timing_style") != "event")
	return;

    for (e = ev.head(); e->next() != 0; e = e->next())
    {
	n = e->next();
	if ((n->F("start") - e->F("end"))  > min_length)
	{
	    dummy = e->insert_after();
//	    dummy->set("end", 19.0);
	    dummy->set("end", n->F("start"));
//	    cout << n->F("start") << endl;
//	    sleep(2);
//	    dummy->set("start", e->F("end"));
//	    cout << *n << endl;
//	    cout << *dummy << endl;
//	    cout << dummy << endl;
//	    sleep(1);
	}
/*	else // make sure starts and ends are now properly contiguous
	{
	    float pos = (n->F("start") - e->F("end")) / 2.0;
//	    cout << "balanced pos = " << pos << endl;
	    e->set("end", pos);
	    n->set("start", pos);
	}
*/
    }
//    cout << "Event to segment\n";
//    cout << ev;
    set_fn_start(ev);

    // REORG - replace by stream feature
    ev.f.set("timing_style", "segment");
//    cout << "Event to segment\n";
//    cout << ev;

}

void remove_rfc_features(EST_Relation &ev)
{
    for (EST_Item *e = ev.head(); e != 0; e = e->next())
    {
	e->f_remove("rfc.rise_amp");
	e->f_remove("rfc.rise_dur");
	e->f_remove("rfc.fall_amp");
	e->f_remove("rfc.fall_dur");
	e->f_remove("rfc.type");
    }
}

void remove_tilt_features(EST_Relation &ev)
{
    for (EST_Item *e = ev.head(); e != 0; e = e->next())
    {
	e->f_remove("tilt.amp");
	e->f_remove("tilt.dur");
	e->f_remove("tilt.tilt");
    }
}

float unit_curve(float amp, float dur, float t)
{
    float val;
    float x;

    x = (t / (dur)) * 2.0;
    if (x < 1.0)
      val = pow(x, float(2.0));
    else
      val = 2 - pow((float(2.0) - x), float(2.0));
	
    val = (val / 2.0);
	
    val *= amp;
    val += 0;		// vert dist.
    
    return val;
}

float fncurve(float length, float t, float curve)
{
    float val;
    float x;
    
    x = (t / length) * 2.0;
    
    if (x < 1.0)
	val = pow(x, curve);
    else
	val = 2 - pow((2 - x), curve);
    
    val = val / 2.0;
    
    return val;
}

int event_item(EST_Item &e)
{	
    return e.I("int_event", 0);
}
int sil_item(EST_Item &e)
{
    return ((e.name() == "sil") || (e.name() == "SIL"));
}
int connection_item(EST_Item &e)
{
    return ((e.name() == "c") || (e.name() == "C"));
}
