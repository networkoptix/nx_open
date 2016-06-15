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
/*             Author :  Paul Taylor and Alan Black                      */
/*             Date   :  June 1996                                       */
/*-----------------------------------------------------------------------*/
/*   EST_Wave class methods for cutting, and extracting                  */
/*                                                                       */
/*=======================================================================*/

#include <cstring>
#include "EST_unix.h"
#include <cstdlib>
#include "EST_cutils.h"
#include "EST_string_aux.h"
#include "EST_Wave.h"
#include "EST_wave_aux.h"
#include "EST_Track.h"
#include "ling_class/EST_Relation.h"
#include "ling_class/EST_item_aux.h"

static int wave_subwave(EST_Wave &subsig,EST_Wave &sig, 
			float offset, float length);

int wave_divide(EST_WaveList &wl, EST_Wave &sig, EST_Relation &keylab,
		const EST_String &ext)
{
    wl.clear();
    EST_Wave a;
    EST_Item *k;
    EST_String filename;
    float start = 0,end;
    
    for (k = keylab.head(); k; k = k->next())
    {
	a.clear();
	end = k->F("end",0);
	if (end < start)
	    continue;
	wave_subwave(a, sig, start, end-start);
	filename = (EST_String)k->f("file");
	a.set_name(filename + ext);
	wl.append(a);
	start = end;
    }

    return 0;
}

int wave_extract(EST_Wave &part, EST_Wave &sig, EST_Relation &keylab, 
		 const EST_String &file)
{
    EST_Wave a;
    EST_Item *k;
    EST_String key_file_name;
    float start=0, end;
    
    for (k = keylab.head(); k; k = k->next())
    {
	end = k->F("end",0);
	key_file_name = (EST_String)k->f("file");
	if (key_file_name == file)
	{
	    wave_subwave(part, sig, start, end-start);
	    return 0;
	}
	start = end;
    }
    cerr << "Couldn't locate file fragment " << file << " in keylab file\n";
    return -1;
}


static int wave_subwave(EST_Wave &subsig,EST_Wave &sig, 
			float offset, float length)
{
    return wave_subwave(subsig, sig, (int)(offset *(float)sig.sample_rate()),
			(int)(length *(float)sig.sample_rate()));
}

int wave_subwave(EST_Wave &subsig,EST_Wave &sig,int offset,int length)
{
    // take out a subpart of sig and put it in subsig
    int ns;

    if (length == -1)
	ns = sig.num_samples() - offset;
    else
	ns = length;
    
    if ((offset+ns) > sig.num_samples())
    {
	cerr << "Subset past end of signal\n";
	return -1;
    }

    EST_Wave subwave;

    sig.sub_wave(subwave, offset, ns, 0, EST_ALL);

    subsig.copy(subwave);

    return 0;
}

int track_divide(EST_TList<EST_Track> &mtfr, EST_Track &fv, EST_Relation &key)
{
    EST_Track a;
    EST_Item  *k, t;
    float kstart, length;
    int i, j, l, n;
    
    mtfr.clear();
    
    if ((key.tail())->F("end") < (fv.t(fv.num_frames() - 1)))
    {
	cerr << "Key file must extend beyond end of EST_Track\n";
	cerr << "key end: " << key.tail()->F("end") << " EST_Track end: " 
	    << fv.t(fv.num_frames() - 1) << endl;
	return -1;
    }
    
    k = key.head();
    a.set_name(k->name());
    kstart = 0.0;
    
    length = end(*k) - kstart;
    n = (int)(length / (float) fv.shift()) + 2;
    a.resize(n, fv.num_channels());
    
    for (i = 0, l = 0; i < fv.num_frames(); ++i, ++l)
    {
	for (j = 0; j < fv.num_channels(); ++j)
	    a(l, j) = fv(i, j);
	
	if (fv.t(i) > k->F("end"))
	{
	    a.set_num_frames(l + 1);
	    mtfr.append(a);
	    
	    kstart = k->F("end");
	    k = k->next();
	    a.set_name(k->name());
	    length = k->F("end") - kstart;
	    n = (int)(length / (float) fv.shift()) + 2;
	    //	    cout << "n frames: " << n << endl;
	    a.resize(n, fv.num_channels());
	    a.fill_time(fv.shift());
	    
	    //	for (j = 0; j < fv.order(); ++j)
	    //	    a(0, j) = fv(i, j);
	    l = -1;
	}
    }
    a.set_num_frames(l);
    mtfr.append(a);
    return 0;
}

