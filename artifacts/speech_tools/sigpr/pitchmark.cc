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
/*                 Author: Paul Taylor                                   */
/*                 Date   :  December 1997                               */
/*-----------------------------------------------------------------------*/
/*                   Pitchmark Laryngograph Signals                      */
/*                                                                       */
/*=======================================================================*/

/* Note - this is based on a pitchmarker developed by Mike Macon and
written in matlab.
*/

#include "stdlib.h"
#include "sigpr/EST_filter.h"
#include "sigpr/EST_pitchmark.h"
#include "ling_class/EST_Relation.h"
#include "EST_math.h"
#include "EST_inline_utils.h"
#include "EST_wave_aux.h"
#include "EST_track_aux.h"


void delta(EST_Wave &tr, EST_Wave &d, int regression_length);

EST_Track pitchmark(EST_Wave &lx, int lx_lf, int lx_lo, int lx_hf, 
		    int lx_ho, int df_lf, int df_lo, int mo, int debug)
{
    EST_Track pm;
    EST_Wave lxdiff;

    pm.set_equal_space(false);
    // pre-filtering

    if (debug)
	cout << "pitchmark 1\n";

    FIRlowpass_double_filter(lx, lx_lf, lx_lo);
    FIRhighpass_double_filter(lx, lx_hf, lx_ho);

    if (debug)
	cout << "pitchmark 2\n";

    if (debug)
	lx.save("tmpfilt.lx");

//    cout << "df " << df_lf << " df_o " << df_lo << endl;

//    lxdiff = lx;
//    differentiate(lxdiff);
    lxdiff.resize(lx.num_samples());
    lxdiff.set_sample_rate(lx.sample_rate());
    delta(lx, lxdiff, 4);

    if (debug)
	lxdiff.save("tmpdiff.lx");

    // it was found that median smoothing worked better here.

    if (df_lo > 0)
	FIRlowpass_double_filter(lxdiff, df_lf, df_lo);

    if (mo > 0)
	simple_mean_smooth(lxdiff, mo);

    if (debug)
	lxdiff.save("tmpfiltdiff.lx");

    neg_zero_cross_pick(lxdiff, pm);

    return pm;
}

EST_Track pitchmark(EST_Wave &lx, EST_Features &op)
{
    EST_Track pm;
    EST_Wave lxdiff;
    int lx_lf, lx_lo, lx_hf, lx_ho, df_lf, df_lo, mo, debug;

    lx_lf = op.present("lx_low_frequency") ? 
	op.I("lx_low_frequency") : 400;
    lx_lo = op.present("lx_low_order") ? 
	op.I("lx_low_order") : 19;

    lx_hf = op.present("lx_high_frequency") ? 
	op.I("lx_high_frequency") : 40;
    lx_ho = op.present("lx_high_order") ? 
	op.I("lx_high_order") : 19;

    df_lf = op.present("df_low_frequency") ? 
	op.I("df_low_frequency") : 1000;
    df_lo = op.present("df_low_order") ? 
	op.I("df_low_order") : 0;

    mo = op.present("median_order") ? 
	op.I("median_order") : 19;

    debug = op.present("pm_debug") ? 1 : 0;

    return pitchmark(lx, lx_lf, lx_lo, lx_hf, lx_ho, df_lf, df_lo, 
		     mo, debug);
}

/** Iterate through track and eliminate any frame whose distance to a
preceding frames is less than min seconds*/

void pm_min_check(EST_Track &pm, float min)
{
    int i, j;

    for (i = j = 0; i < pm.num_frames() - 1; ++i, ++j)
    {
	pm.t(j) = pm.t(i);
	while ((i < (pm.num_frames() - 1)) && ((pm.t(i + 1) - pm.t(i)) < min))
	    ++i;
    }
    if (i < pm.num_frames())
	pm.t(j) = pm.t(i);
    pm.resize(j, pm.num_channels());
}


void pm_fill(EST_Track &pm, float new_end, float max, float min, float def)
{
    EST_FVector new_pm;

    if (new_end < 0)
	new_end = pm.end();

//    if (debug)
    // cout<< "new end:" << new_end << endl;
    // largest possible set of new pitchmarks

//    cout << "num frames:" << pm.num_frames() << endl;
//    cout << "num frames:" << pm.end() << endl;
//    cout << "num frames:" << min << endl;
    new_pm.resize(int(new_end / min));
//    cout << "num frames:" << pm.end()/min << endl;
//    cout << "num frames:" << new_pm.n() << endl;

    int i, j, npm=0;
    float last = 0.0;

    int dropped=0, added=0;

    for(j = 0; j < pm.num_frames(); j++)
    {
	float current = pm.t(j);

	if (current > new_end)
	    break;

	if (current - last < min)
	{
	    // drop current pitchmark
	    dropped++;
	}

	else if (current-last > max)
	{
	    // interpolate
	    int num = ifloor((current - last)/ def);
	    float size = (current-last) / num;
	    for (i = 1; i <= num; i++)
	    {
		new_pm[npm] = last + i * size;
		npm++;
		added++;
	    }
	}
	else
	{
	    new_pm[npm] = pm.t(j);
	    npm++;
	}
	last=current;
    }
    
    if (new_end - last > max)
    {
	// interpolate
	int num = ifloor((new_end - last)/ def);
	float size = (new_end -last) / num;
	for (i = 1; i <= num; i++)
	{
	    new_pm[npm] = last + i * size;
	    npm++;
	    added++;
	}
    }
    
//    if (debug)
//	if (dropped>0 || added >0)
//	    cout << "Dropped " << dropped<< " and added " << added << " PMs\n";

//    if (debug)
    pm.resize(npm, pm.num_channels());
    for (i = 0; i < npm; i++)
	pm.t(i) = new_pm(i);
}

void neg_zero_cross_pick(EST_Wave &lx, EST_Track &pm)
{
    int i, j;
    pm.resize(lx.num_samples(), EST_CURRENT);
    
    for (i = 1, j = 0; i < lx.num_samples(); ++i)
	if ((lx.a(i -1) > 0) && (lx.a(i) <= 0))
	    pm.t(j++) = lx.t(i);
    
    pm.resize(j, EST_CURRENT);

    for (i = 0; i < pm.num_frames(); ++i)
	pm.set_value(i);
}

void pm_to_label(EST_Track &pm, EST_Relation &lab)
{
    EST_Item *seg;
    lab.clear();

    for (int i = 0; i < pm.num_frames(); ++i)
    {
	seg = lab.append();
	seg->set("name","");
	seg->set("end",pm.t(i));
    }    
}

void pm_to_f0(EST_Track &pm, EST_Track &f0)
{
    float prev_pm = 0.0;
    f0 = pm;
    f0.resize(EST_ALL, 1);

    for (int i = 0; i < f0.num_frames(); ++i)
    {
	f0.a(i, 0) = 1.0 / (f0.t(i) - prev_pm);
	prev_pm = f0.t(i);
    }
}

void pm_to_f0(EST_Track &pm, EST_Track &fz, float shift)
{
    int i;
    float period;

    fz.resize((int)(pm.end()/shift), 1);
    fz.fill_time(shift);

    for (i = 0; i < fz.num_frames() -1 ; ++i)
    {
        period = get_time_frame_size(pm, pm.index_below(fz.t(i)));
	fz.a(i) = 1.0 /period;
    }
}
