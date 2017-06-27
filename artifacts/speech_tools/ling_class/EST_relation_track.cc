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
/*                       Author :  Paul Taylor and Simon King            */
/*                       Date   :  June 1995                             */
/*-----------------------------------------------------------------------*/
/*          Stream class auxiliary routines that refer to tracks         */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cmath>
#include "EST_types.h"
#include "ling_class/EST_Relation.h"
#include "ling_class/EST_relation_aux.h"
#include "EST_track_aux.h"
#include "EST_string_aux.h"
#include "EST_io_aux.h"
#include "EST_Option.h"

static int pos_phone(const EST_Relation &seg, float x, float shift);

void track_to_label(const EST_Track &tr, EST_Relation &lab, float thresh)
{
    int i;
    EST_Item *tmp_seg;
    int p_pos = FALSE;
    int c_pos = FALSE;
    
    for (i = 0; i < tr.num_frames(); ++i)
    {
        if (tr.a(i) > thresh)
            c_pos = TRUE;
        else 
            c_pos = FALSE;
        
        if (c_pos == p_pos)
        {
            p_pos = c_pos;
            continue;
        }

        tmp_seg = lab.append();

        if (c_pos == TRUE)
            tmp_seg->set_name("neg");
        else 
            tmp_seg->set_name("pos");

        tmp_seg->set("end", tr.t(i - 1));

        p_pos = c_pos;
    }

    tmp_seg = lab.append();
    if (c_pos)
        tmp_seg->set_name("pos");
    else 
        tmp_seg->set_name("neg");

    tmp_seg->set("end", tr.t(i - 1));
}

void track_to_pm(const EST_Track &tr, int sample_rate, EST_Relation &lab)
{
    int i;
    EST_Item *tmp_seg;
    
    bool have_offset = tr.has_channel(channel_offset);
    bool have_length = tr.has_channel(channel_length);
    
    for (i = 0; i < tr.num_frames(); ++i)
    {
	float c, b, e=0.0;
	if (have_length)
	    if (have_offset)
		get_frame_o(tr, sample_rate, i, b, c, e);
	    else
		get_frame(tr, sample_rate, i, b, c, e);
	else
	    c =  tr.t(i);

	if (have_length)
	{
	    tmp_seg = lab.append();
	    tmp_seg->set_name("b");
	    tmp_seg->set("end", b);
	}

	tmp_seg = lab.append();
	tmp_seg->set_name("pm");
	tmp_seg->set("end", c);

	if (have_length)
	{
	    tmp_seg = lab.append();
	    tmp_seg->set_name("e");
	    tmp_seg->set("end", e);
	}
    }
}

void label_to_track(const EST_Relation &lab, EST_Track &tr,
		    float shift, float offset, float
		    range, float req_l, const EST_String &pad)
{
    EST_Item tmp_seg;
    int i;
    int n, endn;

    n = (int)ceil(lab.tail()->F("end")/shift);
    endn = (req_l > 0.0) ? (int)(req_l /shift) : n;

    //    cout << req_l << endl;
    //    cout << "shift " << shift << endl;
    //    cout << "endn is " << endn << endl;
    // cout << lab.tail()->f.F("end") << " " << shift << endl;

    tr.resize(endn, 1);
    tr.fill_time(shift);

    for (i = 0; i < n; ++i)
    {
	tr.a(i) = (pos_phone(lab, tr.t(i), shift) * range) + offset;
	tr.set_value(i);
    }
    for (; i < endn; ++i)
    {
	tr.a(i) = (pad == "high") ? range + offset : offset;
	tr.set_value(i);
    }
}

void label_to_track(const EST_Relation &lab, 
		    const EST_Option &al, 
		    const EST_Option &op,
		    EST_Track &tr)
{
    float shift = op.present("frame_shift") ? op.fval("frame_shift"): 0.01;
    float offset = op.present("label_offset")? op.fval("label_offset"):0.0;
	
    float range = op.present("label_range") ? op.fval("label_range"): 1.0;
    float length = al.present("-length") ? al.fval("-length") : -1.0;
	
    label_to_track(lab, tr, shift, offset, range, length, al.val("-pad", 0));
    // tr.amin = 0.0;
    // tr.amax = 3.0;

}

static int pos_phone(const EST_Relation &seg, float x, float shift)
{
    // returns true if x is in a positive segment. The decision is
    // slightly biased towards positive inclusion when x is near
    // a boundary.
    EST_Item *p;
    
    for (p = seg.head(); p != 0; p = p->next())
	if (p->f("pos") == 1)
	    if ((x < (p->F("end") + (shift / 2.0))) &&
		(x > (start(p) -  (shift / 2.0))))
		return 1;
    return 0;
}

