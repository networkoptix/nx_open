/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                    Copyright (c) 1994,1995,1996                       */
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
/*                                                                       */
/*                       Author :  Paul Taylor                           */
/*                       Date   :  April 1994                            */
/* -------------------------------------------------------------------   */
/*                      EST_Track Class source file                      */
/*                                                                       */
/*************************************************************************/

#include <fstream>
#include <iostream>
#include <cmath>
#include "EST_unix.h"
#include "EST_Track.h"
#include "EST_string_aux.h"
#include "EST_TrackFile.h"
#include "EST_error.h"

const int EST_Track::default_sample_rate=16000; // occasionally needed for xmg files
const float EST_Track::default_frame_shift=0.005; // default frame spacing.

const EST_String DEF_FILE_TYPE = "est";

EST_Track::EST_Track()
{
    default_vals();
}

EST_Track::EST_Track(const EST_Track &a)
{ 
    default_vals();
    copy(a);
}

EST_Track::EST_Track(int n_frames, int n_channels)
{
    default_vals();
    p_values.resize(n_frames, n_channels);
    p_times.resize(n_frames);
    p_is_val.resize(n_frames);
    p_channel_names.resize(n_channels);

    p_aux.resize(n_frames, 1);
    p_aux_names.resize(1);

    char d = 0;
    p_is_val.fill(d);
}

EST_Track::EST_Track(int n_frames, EST_TrackMap &map)
{
  int n_channels = map.last_channel()+1;

  default_vals();
  p_values.resize(n_frames, n_channels);
  p_times.resize(n_frames);
  p_is_val.resize(n_frames);
  p_channel_names.resize(n_channels);
  char d = 0;
  p_is_val.fill(d);
  assign_map(map);
}

EST_Track::~EST_Track(void)
{
  //  clear_features();
}

void EST_Track::default_channel_names()
{
    for (int i = 0; i < num_channels(); ++i)
	set_channel_name("track" + itoString(i), i);
}

void EST_Track::default_vals(void)
{
    p_equal_space = FALSE;
    p_single_break = FALSE;
    p_values.resize(0, 0);
    p_times.resize(0);
    p_is_val.resize(0);
    p_aux.resize(0, 0);
    p_aux_names.resize(0);
    p_channel_names.resize(0);
    p_map = NULL;
    p_t_offset=0;
    
    init_features();
}

void EST_Track::set_break(int i) // make location i hold a break
{
    if (i >= num_frames())
	cerr << "Requested setting of break value of the end of the array\n";
    
    p_is_val[i] = 1;
}    


void EST_Track::set_value(int i) // make location i hold a value
{
    p_is_val[i] = 0;
}    

const EST_String EST_Track::channel_name(int i, const EST_ChannelNameMap &map, int strings_override) const
{
    (void)map;
    (void)strings_override;
    return p_channel_names(i);
}    

/* OLD ENUM VERSION 

const EST_String EST_Track::channel_name(int i, const EST_ChannelNameMap &map, int strings_override) const
{
    EST_ChannelType type = channel_unknown;
    
    if (strings_override && p_channel_names(i) != "")
	return p_channel_names(i);
    else if (p_map!=NULL && ((type = p_map->channel_type(i)) != channel_unknown))
    {
	const char *name = map.name(type);
	if (!name)
	    name  = EST_default_channel_names.name(type);
	if (name != NULL)
	  return EST_String(name);
	return "unnamed_channel" + itoString(type);
    }
    else if (!strings_override && p_channel_names(i) != "")
	return p_channel_names(i);
    else 
	return "track" + itoString(i);
}    
*/
void EST_Track::set_channel_name(const EST_String &fn, int i)
{
    p_channel_names[i] = fn;
}    

void EST_Track::set_aux_channel_name(const EST_String &fn, int i)
{
    p_aux_names[i] = fn;
}    

ostream& operator << (ostream& s, const EST_Track &tr)
{
    int i, j;
    for (i = 0; i < tr.num_frames(); ++i)
    {
	s << tr.t(i);
	for (j = 0; j < tr.num_channels(); ++j)
	    s << "\t" << tr(i, j);
	for (j = 0; j < tr.num_aux_channels(); ++j)
	    s << "\t" << tr.aux(i, j);
	s << "\t" << !tr.track_break(i) << endl;
    }
    return s;
}

void EST_Track::copy(const EST_Track& a)
{
    copy_setup(a);
    p_values = a.p_values;
    p_times = a.p_times;
    p_is_val = a.p_is_val;
    p_t_offset = a.p_t_offset;
    p_aux = a.p_aux;
    p_aux_names = a.p_aux_names;
}

void EST_Track::copy_setup(const EST_Track& a)
{
    p_equal_space = a.p_equal_space;
    p_single_break = a.p_single_break;
    p_channel_names = a.p_channel_names;
    p_map = a.p_map;
    copy_features(a);
}    

void EST_Track::resize(int new_num_frames, int new_num_channels, bool set)
{
    int old_num_frames = num_frames();

    if (new_num_frames<0)
	new_num_frames = num_frames();

    if (new_num_channels<0)
	new_num_channels = num_channels();

    p_channel_names.resize(new_num_channels);

    // this ensures the new channels have a default name
    if (new_num_channels > num_channels())
	for (int i = num_channels(); i < new_num_channels; ++i)
	    set_channel_name("track_" + itoString(i), i);

    p_values.resize(new_num_frames, new_num_channels, set);
    p_times.resize(new_num_frames, set);
    p_is_val.resize(new_num_frames, set);

    p_aux.resize(new_num_frames, num_aux_channels(), set);
  
    // Its important that any new vals get set to 0
    for (int i = old_num_frames; i < num_frames(); ++i)
	p_is_val.a_no_check(i) = 0;

}

static void map_to_channels(EST_StrList &channel_map, 
		     EST_StrList &channel_names)
{
    EST_Litem *p;
    EST_String b, type, first, last;
    int n_f, n_l;

    for (p = channel_map.head(); p; p = p->next())
    {
	b = channel_map(p);
	if (b.matches("$", 0))
	{
	    // do this backwards as types may have "_" in them
	    b = b.after("$");
	    if (!b.contains("-"))
	    {
		cerr<<"Ill formed coefficient range in map: " << b << "\n";
		return;
	    }
	    type = b.before("-");
	    first = b.after("-");

	    if (!first.contains("+"))
	    {
		cerr<<"Ill formed coefficient range in map: "<<first<<"\n";
		return;
	    }

	    last = first.after("+");
	    first = first.before("+");
		
	    n_f = Stringtoi(first);
	    n_l = Stringtoi(last);

	    for (int i = n_f; i < n_l; ++i)
		channel_names.append(type + "_" + itoString(i));
	    channel_names.append(type + "_N");
	}
	else
	    channel_names.append(b);
    }
}

void EST_Track::resize(int new_num_frames, EST_StrList &new_channels, bool set)
{
    EST_StrList x;
    map_to_channels(new_channels, x);
    int i;
    EST_Litem *p;

    int new_num_channels;
    new_num_channels = x.length();

    if (new_num_frames<0)
	new_num_frames = num_frames();


    p_channel_names.resize(new_num_channels);
    // this ensures the new channels have a default name

    for (i = 0, p = x.head(); p ; p = p->next(), ++i)
	set_channel_name(x(p), i);

    p_values.resize(new_num_frames, new_num_channels, set);
    p_times.resize(new_num_frames, set);
    p_is_val.resize(new_num_frames, set);
  
//  for (int i = 0; i < new_num_frames; ++i)
//    p_is_val.a_no_check(i) = 1;
}

void EST_Track::resize_aux(EST_StrList &new_aux_channels, bool set)
{
    int i;
    EST_Litem *p;

    int new_num_channels;
    new_num_channels = new_aux_channels.length();

    p_aux_names.resize(new_num_channels);

    // this ensures the new channels have a default name
    for (i = 0, p = new_aux_channels.head(); p ; p = p->next(), ++i)
	set_aux_channel_name(new_aux_channels(p), i);

    p_aux.resize(num_frames(), new_num_channels, set);
}

EST_Track& EST_Track::operator+=(const EST_Track &a) // add to existing track
{
    int i, j, k;
    
    if (num_frames() == 0)	// i.e. no existing EST_Track to add to
    {
	*this = a;
	return *this;
    }
    
    if (a.num_channels() != num_channels())
    {
	cerr << "Error: Tried to add " << a.num_channels() << 
	    " channel EST_Track to "<<num_channels() << " channel EST_Track\n";
	return *this;
    }
    
    int old_num = num_frames();
    float old_end = end();
    this->resize(a.num_frames()+ this->num_frames(), this->num_channels());
    for (i = 0, j = old_num; i < a.num_frames(); ++i, ++j)
    {
	for (k = 0; k < num_channels(); ++k)
	    p_values.a_no_check(j, k) = a(i, k);
	p_times[j] = old_end + a.t(i);
	p_is_val[j] = a.p_is_val(i);
    }
    
    return *this;
}

EST_Track& EST_Track::operator|=(const EST_Track &a)
{				// add to existing track in parallel
    int i, j, k;
    
    if (num_channels() == 0)	// i.e. no existing EST_Track to add to
    {
	*this = a;
	return *this;
    }
    
    if (a.num_frames() != num_frames())
    {
	cerr << "Error: Tried to add " << a.num_frames() << 
	    " channel EST_Track to "<<num_frames()<< " channel EST_Track\n";
	return *this;
    }
    
    int old_num = num_channels();
    this->resize(a.num_frames(), this->num_channels() + a.num_channels());
    for (i = 0, j = old_num; i < a.num_channels(); ++i, ++j)
	for (k = 0; k < num_frames(); ++k)
	    p_values.a_no_check(k, j) = a(k, i);
    
    return *this;
}

EST_Track &EST_Track::operator=(const EST_Track& a)
{
    copy(a);
    return *this;
}    


int EST_Track::channel_position(const char *name, int offset) const
{
    int c;
    
    for (c=0; c<num_channels(); c++)
	if (channel_name(c) == name)
	    return c+offset;
    
    return -1;
}

float &EST_Track::a(int i, const char *name, int offset)
{ 
    int c;
    
    for (c=0; c<num_channels(); c++)
	if (channel_name(c) == name)
	    return p_values.a_no_check(i, c+offset);
    
    cerr << "no channel '" << name << "'\n";
    return *(p_values.error_return);
}

EST_Val &EST_Track::aux(int i, const char *name)
{ 
    for (int c = 0; c < num_aux_channels(); c++)
	if (aux_channel_name(c) == name)
	    return p_aux.a_no_check(i, c);
    
    cerr << "no auxiliary channel '" << name << "' found\n";
    return *(p_aux.error_return);
}

EST_Val &EST_Track::aux(int i, int c)
{ 
    return p_aux(i, c);
}

EST_Val &EST_Track::aux(int i, int c) const
{ 
    return ((EST_Track *)this)->aux(i,c);
}

#define EPSILON (0.0001)

float &EST_Track::a(float t, int c, EST_InterpType interp)
{
    static float ia = 0.0;
    
    if (interp == it_nearest)
	return p_values.a_no_check(index(t), c);
    else if (interp == it_linear)
    {
	int i = index_below(t);
	if (i < 0)
	    return a(0,c);
	
	float n = a(i,c), n1 = a(i+1,c);
	float tn = p_times(i), tn1 = p_times(i+1);
	ia = n + (n1-n)*(t-tn)/(tn1-tn);
	return ia;
    }
    else if (interp == it_linear_nz)
    {
	int i = index_below(t);
	if (i < 0)
	    return a(0,c);
	
	float n = a(i,c), n1 = a(i+1,c);
	if (fabs(n) < EPSILON || fabs(n1) < EPSILON)
	    return p_values.a_no_check(index(t), c);
	float tn = p_times(i), tn1 = p_times(i+1);
	ia = n + (n1-n)*(t-tn)/(tn1-tn);
	return ia;
    }
    return ia;
}

int EST_Track::index(float x) const
{
    if (equal_space())
    {
	float s = shift();
	int f = (int)( (x-t(0))/s+0.5 ); //don't assume track starts at t=0.0
	if (f<0)
	    f=0;
	else if (f>= num_frames())
	    f=num_frames()-1;
	return f;
    }
    else if (num_frames() > 1) //if single frame, return that index (0)
    {
	int bst, bmid, bend;
	bst = 1;
	bend = num_frames();
	if (x < p_times.a_no_check(bst))
	    bmid=bst;
	if (x >= p_times.a_no_check(bend-1))
	    bmid=bend-1;
	else
	{
	    while (1)
	    {
		bmid = bst + (bend-bst)/2;
		if (bst == bmid)
		    break;
		else if (x < p_times.a_no_check(bmid))
		{
		    if (x >= p_times.a_no_check(bmid-1))
			break;
		    bend = bmid;
		}
		else
		    bst = bmid;
	    }
	}
	if (fabs(x - p_times.a_no_check(bmid)) < 
	    fabs(x - p_times.a_no_check(bmid-1)))
	    return bmid;
	else
	    return bmid - 1;
    }
    
    return num_frames() -1;
}

int EST_Track::index_below(float x) const
{
    if (equal_space())
    {
	float s = shift();
	int f = (int)(x/s);
	if (f<0)
	    f=0;
	else if (f>= num_frames())
	    f=num_frames()-1;
	return f;
    }
    else
    {
	for (int i = 1; i < num_frames(); ++i)
	    if (x <= p_times.a_no_check(i))
		return i - 1;
	return num_frames()-1;
    }
}

int EST_Track::val(int i) const
{
    return !p_is_val(i);
}
/*
   "p_equal_space" indicates whether the x-axis values are evenly spaced
   (FIXED) or spaced arbitrarily (VARI).
   
   "p_single_break" describes the break format. F0 contours are seldom
   continuous - often breaks occur due to unvoicing etc. These are a
   marked  in the data arrays by break values, "i_break" for ints
   and "f_break" for floats. The "p_single_break" channel specifies whether
   a break is represented by a single break value, or as a break
   value for every frame. eg
   (SINGLE)
   800  100
   810  105
   BREAK BREAK
   850  130
   860  135
   or
   (MANY)
   800  100
   810  105
   820 BREAK
   830 BREAK
   840 BREAK
   850  130
   860  135
   
   In the MANY case, only the y value is specified as a break, in the
   SINGLE case the x value may or may not be specified as a break. For
   this reason, when checking for breaks, it is useful to only rely
   on the y value being set to the i_break value. Not that if the single_break
   is MANY and the equal_space is FIXED, you dont really need x-axis
   values.
   
   Different functions naturally work better on different representations
   and that is why all these different types are supported. A
   general function mod_cont() is supplied to change from one
   type to another. Not all conversions are currently
   supported however.
   
   */

float EST_Track::end() const
{
    if (num_frames() == 0)
	return 0.0;
    else
	return (p_times(prev_non_break(num_frames())));
}
float EST_Track::start() const
{
    if (num_frames() == 0)
	return 0.0;
    else
	return (track_break(0) ? p_times(next_non_break(0)) : p_times(0));
}

float EST_Track::shift() const
{
    int j1 = 0;
    int j2 = 0;
    
    if (!p_equal_space)
	EST_error("Tried to take shift from non-fixed contour\n");

    do
    {
	j1 = next_non_break(++j1);
	j2 = next_non_break(j1);
	//	cout << "j1:" << j1 << " j2:" << j2 << endl;
    }
    while ((j2 != 0) && (j2 != (j1 +1)));
    
    if (j2 == 0)
    {
	if (num_frames() > 1)
	    return p_times(1) - p_times(0);
	else
	    EST_error("Couldn't determine shift size\n");	    

    }
    return (p_times(j2) - p_times(j1));
}

/* tries to find the next value that isnt a break. Dont really
   know what to do on a fail, so just return 0 */

int EST_Track::next_non_break(int j) const
{
    int i = j;
    for (++i; i < num_frames(); ++i)
    {
	//	cout << "i: " << i << " " << value[i] << endl;
	if (!track_break(i))
	    return i;
    }
    
    return 0;
}

/* give the current point, returns the previous non-break */

int EST_Track::prev_non_break(int j) const
{
    int i = j;
    for (--i; i >= 0 ; --i)
	if (!track_break(i))
	    return i;
    return 0;
}

void EST_Track::change_type(float nshift, bool single_break)
{
    if (nshift != 0.0)
    {
	if (!p_equal_space || nshift != shift())
	    sample(nshift);
	p_equal_space = TRUE;
    }
    
    if (single_break != p_single_break)
    {
	if (!p_single_break)
	    pad_breaks();
	else 
	    rm_excess_breaks();
    }
}

void EST_Track::sample(float f_interval)
{
    EST_FVector new_times;
    EST_FMatrix new_values;
    EST_CVector new_is_break;
    int i, j, n;
    
    n = (int) rint(((end())/ f_interval));
    
    new_times.resize(n);
    new_values.resize(n, num_channels());
    new_is_break.resize(n);
    
    // REORG - can this be replaced with fill_time()?
    for (i = 0; i < n; ++i)
	new_times[i] = (float) ((i + 1) * f_interval);
    
    for (i = 0; i < n; ++i)
    {
	new_is_break[i] = !interp_value(new_times(i), f_interval);
	for (j = 0; j < num_channels(); ++j)
	    new_values(i, j) = !new_is_break(i) ? interp_amp(new_times(i), j, f_interval): 0.0;
    }
    
    p_times = new_times;
    p_values = new_values;
    p_is_val = new_is_break;
    p_single_break = FALSE;
    p_equal_space = TRUE;
}

float EST_Track::interp_amp(float x, int c, float fl)
{
    int i;
    float x1, x2, y1, y2, m;
    
    for (i = 0; i < num_frames(); ++i)
	if ((p_times(i) + (fl / 2.0))> x)
	    break;
    
    if (i == num_frames())
	return p_values.a_no_check(i - 1,c);
    if (i == 0)
	return p_values.a_no_check(0, c);
    
    if (track_break(i) && track_break(i - 1))
	return 0.0;
    
    if (track_break(i))
	return p_values.a_no_check(i - 1, c);
    
    else if (track_break(i - 1))
	return p_values.a_no_check(i, c);
    
    x1 = p_times(i - 1);
    y1 = p_values.a_no_check(i - 1, c);
    x2 = p_times(i);
    y2 = p_values.a_no_check(i, c);
    
    m =  (y2 - y1) / (x2 -x1);
    return ((x - x1) * m) + y1;
}		

int EST_Track::interp_value(float x, float fl)
{
    int i;
    int p, n;
    float cf;
    
    if (p_equal_space)
	cf = shift();
    else
	cf = estimate_shift(x);
    
    for (i = 0; i < num_frames(); ++i)
	if ((p_times(i) + (fl / 2.0))> x)
	    break;
    // This was:    
    //    for (i = 0; i < num_frames(); ++i)
    //	if (p_times[i] > x)
    //	    break;
    
    if (i == 0)			// must be a break for the first value. (can't have i -1).
	return FALSE;
    
    if ((!track_break(i)) && (!track_break(i -1)))
	return TRUE;
    
    p = prev_non_break(i);
    n = next_non_break(i);
    
    if ((x < p_times(p) + (cf / 2.0)) || (x > p_times(n) - (cf / 2.0)))
	return TRUE;		// rounding at edges
    
    return FALSE;
}

float EST_Track::estimate_shift(float x)
{
    int i, j;
    for (j = 0; j < num_frames(); ++j)
	if (p_times(j) > x)
	    break;
    
    for (i = j; i > 0; --i)
	if ((!track_break(i)) && (!track_break(i - 1)))
	    return p_times(i) - p_times(i - 1);
    
    for (i = j; i < num_frames() - 1; ++i)
	if ((!track_break(i)) && (!track_break(i + 1)))
	    return p_times(i + 1) - p_times(i);
    
    return 5.0;			// default value
}

void EST_Track::fill_time( float t, int start )
{
  unsigned int nframes = num_frames();
  
  for( unsigned int i=0; i<nframes; ++i )
    p_times.a_no_check(i) = t * (float) (i + start);
}

void EST_Track::fill_time( float t, float startt )
{
  unsigned int nframes = num_frames();
  
  for( unsigned int i=0; i<nframes; ++i )
    p_times.a_no_check(i) = startt + (t * (float)i);
}

void EST_Track::fill_time( const EST_Track &t )
{
  unsigned int nframes = num_frames();

  for( unsigned int i=0; i<nframes; ++i )
    p_times.a_no_check(i) = t.t(i);
}

void EST_Track::rm_excess_breaks()
{
    int i, j, k;
    EST_FVector new_times;
    EST_CVector new_is_break;
    EST_FMatrix new_values;
    
    new_values.resize(num_channels(), num_frames());
    new_times.resize(num_frames());
    new_is_break.resize(num_frames());
    
    for (i = 0; track_break(i); ++i); //rm leading breaks
    
    for (j = 0; i < num_frames(); ++i, ++j)
    {
	for (k = 0; k < num_channels(); ++k)
	    new_values(j, k) = p_values.a_no_check(i, k);
	new_times[j] = p_times(i);
	new_is_break[j] = p_is_val(i);
	while ((!new_is_break(j)) && (!val(i + 1)))
	    ++i;
    }
    p_times = new_times;
    p_values = new_values;
    p_is_val = new_is_break;
    for (--j; track_break(j); --j) // "rm" trailing breaks
	;
    p_times.resize(num_frames());
    p_values.resize(num_frames(), num_channels());
    p_is_val.resize(num_frames());
    
    p_single_break = TRUE;
}    

void EST_Track::rm_trailing_breaks()
{
    if (num_frames() <=0 )
	return;
    int start, end;

    for (start = 0; start < num_frames(); ++start)
	if (!track_break(start))
	    break;

    for(end=num_frames(); end>0; end--)
      if (!track_break(end-1))
	    break;

    if (start==0 && end==num_frames())
      return;
    
    for (int i=start, j = 0; i < end; ++i, ++j)
    {
	p_times[j] = p_times(i);
	for (int k = 0; k < num_channels(); k++)
	    a_no_check(j, k) = a_no_check(i, k);
	p_is_val[j] = p_is_val(i);
    }

    p_values.resize(end-start, EST_CURRENT, 1);
    
    p_times.resize(num_frames());
    p_is_val.resize(num_frames());
}    

void EST_Track::add_trailing_breaks()
{
    int i, j, k;
    EST_FVector new_times;
    EST_FMatrix new_values;
    int new_num = num_frames();
    
    if (!track_break(0))
	new_num++;
    if (!track_break(num_frames() - 1))
	new_num++;
    
    if (new_num == num_frames()) /*ie trailing breaks already there */
	return;
    
    new_times.resize(new_num);
    new_values.resize(num_channels(), new_num);
    
    j = 0;
    if (!track_break(j))
	set_break(j);
    
    for (i = 0; i < num_frames(); ++i, ++j)
    {
	new_times[j] = p_times(i);
	for (k = 0; k < num_channels(); ++k)
	    new_values(j, k) = p_values.a_no_check(i, k);
    }
    
    if (!track_break(num_frames() - 1))
	set_break(j);
    
    p_times = new_times;
    p_values = new_values;
    p_times.resize(num_frames());
    p_values.resize(num_frames(), num_channels());
}    

void EST_Track::pad_breaks()
{
    if (!p_single_break)
	return;
    
    if (!p_equal_space)
	EST_error("pad_breaks: Can only operate on fixed data\n");
    
    EST_FVector new_times;
    EST_FMatrix new_values;
    EST_CVector new_is_break;
    int i, j, k, n;
    
    n = (int)(((end())/ shift()) + 1.0);
    int s = int(start()/ shift());
    
    for (i = 0; i < n; ++i)
    {
	new_times[i] = (float) (i * shift());
	for (k = 0; k < num_channels(); ++k)
	    new_values(k, i) = 0.0;
	new_is_break[i] = 0;
    }
    
    for (i = 0, j = s; j < n; ++i, ++j)
    {
	if (track_break(i))
	{
	    for (; new_times(j) < p_times(i + 1); ++j);
	    --j;
	}
	else
	{
	    new_is_break[j] = 1;
	    for (k = 0; k < num_channels(); ++k)
		new_values(j, k) = p_values.a_no_check(i, k);
	}
    }
    new_is_break[j] = 1;
    for (k = 0; k < num_channels(); ++k)
	new_values(j, k) = p_values.a_no_check(i, k);
    
    p_times = new_times;
    p_values = new_values;
    p_is_val = new_is_break;
    
    p_times.resize(num_frames());
    p_is_val.resize(num_frames());
    p_values.resize(num_frames(), num_channels());
    
    p_single_break = FALSE;
}    

static bool bounds_check(const EST_Track &t, int f, int c, int set)
{
  const char *what = set? "set" : "access";

  if (f<0 || f >= t.num_frames())
    {
      cerr << "Attempt to " << what << " frame " << f << " of " << t.num_frames() << " frame track\n";
      return FALSE;
    }
  if (c<0 || c >= t.num_channels())
    {
      cerr << "Attempt to " << what << " channel " << c << " of " << t.num_channels() << " channel track\n";
      return FALSE;
    }

return TRUE;
}

static bool bounds_check(const EST_Track &t, 
			 int f, int nf,
			 int c, int nc,
			 int set)
{
  const char *what = set? "set" : "access";

  if (nf>0)
    {
      if (f<0 || f >= t.num_frames())
	{
	  cerr << "Attempt to " << what << " frame " << f << " of " << t.num_frames() << " frame track\n";
	  return FALSE;
	}
      if (f+nf-1 >= t.num_frames())
	{
	  cerr << "Attempt to " << what << " frame " << f+nf-1 << " of " << t.num_frames() << " frame track\n";
	  return FALSE;
	}
    }

  if (nc>0)
    {
      if (c<0 || c >= t.num_channels())
	{
	  cerr << "Attempt to " << what << " channel " << c << " of " << t.num_channels() << " channel track\n";
	  return FALSE;
	}
      if (c+nc-1 >= t.num_channels())
	{
	  cerr << "Attempt to " << what << " channel " << c+nc-1 << " of " << t.num_channels() << " channel track\n";
	  return FALSE;
	}
    }

return TRUE;
}

float &EST_Track::a(int i, int c)
{
  if (!bounds_check(*this, i,c,0))
      return *(p_values.error_return);

  return p_values.a_no_check(i,c);
}

float EST_Track::a(int i, int c) const
{
  return ((EST_Track *)this)->a(i,c);
}

int EST_Track::empty() const
{
    int i, num;
    
    for (i = num = 0; i < num_frames(); ++i)
	if (val(i))
	    return 0;		// i.e. false
    
    return 1;			// i.e. true
}

void EST_Track::channel(EST_FVector &cv, const char * name, int startf, int nf)
{
    int n;
    if ((n = channel_position(name)) == -1)
    {
	cerr << "No such channel " << name << endl;
	return;
    }
    channel(cv, n, startf, nf);
}

void EST_Track::sub_track(EST_Track &st,
			  int start_frame, int nframes,
			  const EST_String &start_chan_name, int nchans)
{
    int start_chan;
    if (start_chan_name == "")
	start_chan = 0;

    if ((start_chan = channel_position(start_chan_name)) == -1)
	EST_error("sub_track: No such channel %s\n", 
		  (const char *)start_chan_name);

    sub_track(st, start_frame, nframes, start_chan, nchans);
}

void EST_Track::sub_track(EST_Track &st,
			  int start_frame, int nframes,
			  const EST_String &start_chan_name,
			  const EST_String &end_chan_name) 
{
    int start_chan, end_chan, nchans=0;

    if ((start_chan = channel_position(start_chan_name)) == -1)
	EST_error("sub_track: No such channel %s\n", 
		  (const char *)start_chan_name);

    if (end_chan_name == "")
	nchans = EST_ALL;
    else
    {
	if ((end_chan = channel_position(end_chan_name)) == -1)
	    EST_error("sub_track: No such channel %s\n", 
		      (const char*)end_chan_name);
	else
	    nchans = end_chan - start_chan + 1;
    }

    sub_track(st, start_frame, nframes, start_chan, nchans);
}

void EST_Track::sub_track(EST_Track &st,
			  int start_frame, int nframes,
			  int start_chan, int nchans)
{
  if (nframes <0)
    nframes = num_frames() - start_frame;
  if (nchans <0)
    nchans = num_channels() - start_chan;

  if (!bounds_check(*this, start_frame, nframes, start_chan, nchans, 0))
    return;

  p_values.sub_matrix(st.p_values, start_frame, nframes, start_chan, nchans);

  p_times.sub_vector(st.p_times, start_frame, nframes);

  p_is_val.sub_vector(st.p_is_val, start_frame, nframes);

  p_channel_names.sub_vector(st.p_channel_names,   start_chan, nchans);

  // All auxiliary information is included. These are effectively
  // pointer statements

  p_aux.sub_matrix(st.p_aux, start_frame, nframes, 0, EST_ALL);
  p_aux_names.sub_vector(st.p_aux_names, 0, EST_ALL);

  st.p_t_offset = p_t_offset;
  
  st.p_equal_space = p_equal_space;
  st.p_single_break = p_single_break;
  st.copy_features(*this);

  if (p_map!=0)
    st.p_map = new EST_TrackMap(p_map, start_chan, EST_TM_REFCOUNTED);
  else
    st.p_map = NULL;
}


void EST_Track::copy_sub_track(EST_Track &st,
			       int start_frame, int nframes,
			       int start_chan, int nchans) const
{
  if (nframes <0)
    nframes = num_frames() - start_frame;
  if (nchans <0)
    nchans = num_channels() - start_chan;

  if (!bounds_check(*this, start_frame, nframes, start_chan, nchans, 0))
    return;

  st.resize(nframes, nchans);

  for (int ff=0; ff<nframes; ff++)
    {
      st.p_times.a(ff) = p_times.a(ff+start_frame);
      st.p_is_val.a(ff) = p_is_val.a(ff+start_frame);
      for (int c=0; c<nchans; c++)
	st.p_values.a(ff,c) = p_values.a(ff+start_frame,c+start_chan);
    }

  for (int c=0; c<nchans; c++)
    st.p_channel_names.a(c) = p_channel_names.a(c+start_chan);

  st.p_aux = p_aux;
  st.p_aux_names = p_aux_names;

  st.p_equal_space = p_equal_space;
  st.p_single_break = p_single_break;

  st.copy_features(*this);

  if (p_map!=0)
    st.p_map = new EST_TrackMap(p_map, start_chan, EST_TM_REFCOUNTED);
  else
    st.p_map = NULL;
}

void EST_Track::copy_sub_track_out( EST_Track &st, const EST_FVector& frame_times ) const
{
  int f_len = frame_times.length();
  int nchans = num_channels();
  st.resize( f_len, nchans );

  for( int i=0; i<f_len; ++i ){
    
    int source_index = index(frame_times(i));
    
    st.p_times.a(i) = p_times.a( source_index );
    st.p_is_val.a(i) = p_is_val.a( source_index );
      
    for( int c=0; c<nchans; c++ )
      st.p_values.a(i,c) = p_values.a(source_index,c);
  }
  
  st.copy_setup( *this );
  st.set_equal_space( false ); //might not be true, but it's a better default
  
  //  st.p_aux = p_aux;
  //  st.p_aux_names = p_aux_names;
}

void EST_Track::copy_sub_track_out( EST_Track &st, const EST_IVector& frame_indices ) const
{
  int f_len = frame_indices.length();
  int nchans = num_channels();
  st.resize( f_len, nchans );

  int last_index = num_frames()-1;
  
  for( int i=0; i<f_len; ++i ){

    int source_index = frame_indices(i);

    if( source_index <= last_index ){

      st.p_times.a(i) = p_times.a( source_index );
      st.p_is_val.a(i) = p_is_val.a( source_index );

      for( int c=0; c<nchans; c++ )
	st.p_values.a(i,c) = p_values.a(source_index,c);
    }
  }

  st.copy_setup( *this );
  st.set_equal_space( false ); //might not be true, but it's a better default

  //  st.p_aux = p_aux;
  //  st.p_aux_names = p_aux_names;
}



EST_write_status EST_Track::save(const EST_String filename, 
				 const EST_String type)
{
    EST_String save_type = (type == "") ? DEF_FILE_TYPE : type;

    EST_TrackFileType t = EST_TrackFile::map.token(save_type);
    
    if (t == tff_none)
    {
	cerr << "Unknown Track file type " << save_type << endl;
	return write_fail;
    }
    
    EST_TrackFile::Save_File * s_fun = EST_TrackFile::map.info(t).save;
    
    if (s_fun == NULL)
    {
	cerr << "Can't save tracks to files type " << save_type << endl;
	return write_fail;
    }
    
    return (*s_fun)(filename, *this);
}

EST_write_status EST_Track::save(FILE *fp, const EST_String type)
{
    EST_TrackFileType t = EST_TrackFile::ts_map.token(type);
    
    if (t == tff_none)
    {
	cerr << "Unknown Track file type " << type << endl;
	return write_fail;
    }
    
    EST_TrackFile::Save_TokenStream * s_fun = 
	EST_TrackFile::ts_map.info(t).save;
    
    if (s_fun == NULL)
    {
	cerr << "Can't save tracks to files type " << type << endl;
	return write_fail;
    }
    return (*s_fun)(fp, *this);
}

EST_read_status EST_Track::load(EST_TokenStream &ts, float ishift, float startt)
{
    EST_read_status stat = read_error;
    
    for (int n = 0; n < EST_TrackFile::ts_map.n(); n++)
    {
	EST_TrackFileType t = EST_TrackFile::ts_map.token(n);
	
	if (t == tff_none)
	    continue;

	EST_TrackFile::TS_Info *info = &(EST_TrackFile::ts_map.info(t));
	
	if (! info->recognise)
	    continue;
	
	EST_TrackFile::Load_TokenStream * l_fun =info->load;
	
	if (l_fun == NULL)
	    continue;
	
	stat = (*l_fun)(ts, *this, ishift, startt);
	
	if (stat != read_format_error)
	  {
	    if (stat == read_ok)
	      set_file_type(t);
	    break;
	  }
    }
    
    return stat;
}

EST_read_status EST_Track::load(const EST_String filename, float ishift, float startt)
{
    EST_read_status stat = read_error;
    
    for(int n=0; n< EST_TrackFile::map.n() ; n++)
    {
	EST_TrackFileType t = EST_TrackFile::map.token(n);
	
	if (t == tff_none)
	    continue;
	
	
	EST_TrackFile::Info *info = &(EST_TrackFile::map.info(t));
	
	if (! info->recognise)
	    continue;
	
	EST_TrackFile::Load_File * l_fun =info->load;
	
	if (l_fun == NULL)
	    continue;
	
	stat = (*l_fun)(filename, *this, ishift, startt);
	
	if (stat == read_ok)
	{
	    set_file_type(t);
	    break;
	}
	else if (stat == read_error)
	    break;
    }
    
    return stat;
}

EST_read_status EST_Track::load(const EST_String filename, const EST_String type, float ishift, float startt)
{
    EST_TrackFileType t = EST_TrackFile::map.token(type);
    
    if (t == tff_none)
    {
	cerr << "Unknown Track file type " << type << endl;
	return read_error;
    }
    
    EST_TrackFile::Load_File * l_fun = EST_TrackFile::map.info(t).load;
    
    if (l_fun == NULL)
    {
	cerr << "Can't load tracks from file type" << type << endl;
	return read_error;
    }
    
    set_file_type(t);
    return (*l_fun)(filename, *this, ishift, startt);
}

EST_write_status EST_Track::save_channel_names(const EST_String filename)
{
    FILE *file;
    
    if ((file=fopen(filename, "wb"))==NULL)
	return write_fail;
    
    for(int c=0; c<num_channels(); c++)
	fprintf(file, "%s\n", (const char *)channel_name(c));
    
    fclose(file);
    
    return write_ok;
}

EST_read_status EST_Track::load_channel_names(const EST_String filename)
{
    FILE *file;
    static const int buffer_length = 100;
    char buffer[buffer_length];
    
    if ((file=fopen(filename, "rb"))==NULL)
	return misc_read_error;
    
    for(int c=0; c<num_channels(); c++)
    {
	if (!fgets(buffer, buffer_length, file))
	    break;
	
	buffer[strlen(buffer)-1] = '\0';
	set_channel_name(buffer, c);
    }
    
    
    fclose(file);
    
    return format_ok;
}

/* code from here down should be deleted once tracp mapping is modified */


float &EST_Track::a(float t, EST_ChannelType type, EST_InterpType interp)
{ 
    short c = NO_SUCH_CHANNEL;
    
    if (p_map!=0 && (c = p_map->get(type)) != NO_SUCH_CHANNEL)
	return a(t, c, interp);
    else
    {
	cerr << "no channel '" << EST_default_channel_names.name(type) << "' = " << (int)type << "\n";
    }
    return *(p_values.error_return);
}


void EST_Track::assign_map(EST_TrackMap::P map)
{
    p_map = map;
}

void EST_Track::create_map(EST_ChannelNameMap &names)
{
    EST_TrackMap::P map = new EST_TrackMap(EST_TM_REFCOUNTED);
    
    for (int i = 0; i < num_channels(); i++)
    {
	EST_ChannelType type = names.token(p_channel_names(i));
	
	if (type != channel_unknown)
	  map->set(type, i);
    }
    
    assign_map(map);
}


void EST_Track::resize(int new_num_frames, EST_TrackMap &map)
{
    resize(new_num_frames, map.last_channel()+1);
    assign_map(map);
}



int EST_Track::channel_position(EST_ChannelType type, int offset) const
{ 
    if (p_map!=0)
    {
	int p = (*p_map)(type);
	return (p!= NO_SUCH_CHANNEL)?(p+offset): NO_SUCH_CHANNEL;
    }
    return channel_position(EST_default_channel_names.name(type), offset);
}

float &EST_Track::a(int i, EST_ChannelType type, int offset)
{ 
    short c = NO_SUCH_CHANNEL;
    
    if (p_map!=0 && ((c = p_map->get(type)) != NO_SUCH_CHANNEL))
	return p_values.a_no_check(i, c+offset);
    else
    {
	cerr << "no channel '" << EST_default_channel_names.name(type) << "' = " << (int)type << "\n";
    }
    
    return *(p_values.error_return);
}

EST_Track::IPointer_f::IPointer_f()
{
  frame = new EST_Track();
}

EST_Track::IPointer_f::IPointer_f(const IPointer_f &p)
{
  frame=new EST_Track(*(p.frame));
}

EST_Track::IPointer_f::~IPointer_f()
{
  if (frame != NULL)
    {
      delete frame;
      frame=NULL;
    }
}


#if defined(INSTANTIATE_TEMPLATES)

Instantiate_TIterator_T(EST_Track, EST_Track::IPointer_f, EST_Track, Track_itt)

#endif
