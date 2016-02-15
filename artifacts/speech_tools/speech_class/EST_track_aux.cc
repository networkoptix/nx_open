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
/*                      Author :  Paul Taylor                            */
/*                      Date   :  August 1995                            */
/*-----------------------------------------------------------------------*/
/*                   EST_Track Auxiliary routines                        */
/*                                                                       */
/*=======================================================================*/

#include <cmath>
#include <cstdlib>
#include "EST_cutils.h"
#include "EST_simplestats.h"
#include "EST_sort.h"
#include "EST_Track.h"
#include "EST_TrackFile.h"
#include "EST_Option.h"
#include "EST_track_aux.h"
#include "EST_error.h"

//static inline int irint(float f) { return (int)(f+0.5); }
//static inline int irint(double f) { return (int)(f+0.5); }
//static inline int ifloor(float f) { return (int)(f); }

float correlation(EST_Track &a, EST_Track &b, int cha, int chb);

/* Allow EST_Track to be used in an EST_Val */
VAL_REGISTER_CLASS(track,EST_Track)

static int sorttest(const void *a, const void *b)
{ // for use with qsort C library function.
    float *c = (float *)a;
    float *d = (float *)b;
    float res = (*c - *d);
    if (res == 0.0)
	return 0;
    return (res < 0.0) ? -1 : 1;
}

void track_smooth(EST_Track &c, float x, EST_String stype)
{
    if (stype == "median")
	time_med_smooth(c, x);
    else 
	time_mean_smooth(c, x);
}

void time_med_smooth(EST_Track &c, float x)
{
  if (!c.equal_space())
    {
	cerr << "Error: Time smoothing can only operate on fixed contours\n";
	return;
    }
    // want to check for divide by zero
    if (c.shift() == 0.0)
    {
	cerr << "Error in smoothing: time spacing problem\n";
	return;
    }
    int n = (int)(x / c.shift());
    for (int i = 0; i < c.num_channels(); ++i)
	simple_med_smooth(c, n, i);
}

void time_mean_smooth(EST_Track &c, float x)
{
    int j;
    EST_Track t;
    int n = (int)(x / c.shift());

    for (j = 0; j < c.num_channels(); ++j)
	simple_mean_smooth(c, n, j);
}

void simple_med_smooth(EST_Track &c, int n, int channel) 
{// simple median smoother of order n


    // windows longer than twice the track length cause problems
    // here is one solution
    if(n > c.num_frames())
	n=c.num_frames();

    // and tiny windows don't work either
    // can't do median of 2 of fewer points
    if(n < 3)
	return;

    int i, j, h, k;
    float *a = new float[c.num_frames()];
    float *m = new float[n];
    h = n/2;
    

    // sort start using < n order smoothing
    for (i = 0; i < h; ++i)
    {
	k = (i * 2) + 1;
	for (j = 0; j < k; ++j)
	    m[j] = c.a(j, channel);
	qsort(m, k, sizeof(float), sorttest);
	a[i] = m[i];
    }
    
    // sort main section using n order smoothing
    for (i = h; i < c.num_frames() - h; ++i)
    {
	for (j = 0; j < n; ++j)
	    m[j] = c.a(i - h + j, channel);
	
	qsort(m, n, sizeof(float), sorttest);
	a[i] = m[h];
    }
    // sort end section using < n order smoothing
    for (; i < c.num_frames(); ++i)
    {
	k = ((c.num_frames() - i)* 2) -1;
	for (j = 0; j < k; ++j)
	    m[j] = c.a(i - (k/2) + j, channel);
	qsort(m, k, sizeof(float), sorttest);
	a[i] = m[k/2];
    }
    
    for (i = 0; i < c.num_frames(); ++i)
	c.a(i,channel) = a[i];
    
    delete [] a;
    delete [] m;
}

void simple_mean_smooth(EST_Track &c, int n, int channel)
{ // simple mean smoother of order n
    int i, j, h, k=1;
    float *a = new float[c.num_frames()];
    float sum;
    h = n/2;
    
    for (i = 0; i < h; ++i)
    {
	k = (i * 2) + 1;
	sum = 0.0;
	for (j = 0; j < k; ++j)
	    sum += c.a(j, channel);
	a[i] = sum /(float) k;
    }

    k= h*2 + 1;
    
    for (i = h; i < c.num_frames() - h; ++i)
    {
	sum = 0.0;
	for (j = 0; j < k; ++j)
	    sum += c.a(i - h + j, channel);
	a[i] = sum /(float) k;
    }
    
    for (; i < c.num_frames(); ++i)
    {
	k = ((c.num_frames() - i)* 2) -1;
	sum = 0.0;
	for (j = 0; j < k; ++j)
	    sum += c.a(i - (k/2) + j, channel);
	a[i] = sum /(float) k;
    }
    
    for (i = 0; i < c.num_frames(); ++i)
	c.a(i,channel) = a[i];
    
    delete [] a;
}

void absolute(EST_Track &tr)
{
    int i, j;
    for (i = 0; i < tr.num_frames(); ++i)
	for (j = 0; j < tr.num_channels(); ++j)
	    tr.a(i, j) = fabs(tr.a(i, j));
}

void normalise(EST_Track &tr)
{
    EST_FVector mean, sd;
    
    meansd(tr, mean, sd);
    normalise(tr, mean, sd, -1.0, 1.0);
}

/* Normalise a list of tracks */
void normalise(EST_TrackList &trlist, EST_FVector &mean, EST_FVector &sd, 
	       float upper, float lower)
{
    for (EST_Litem *p = trlist.head(); p; p = p->next())
	normalise(trlist(p), mean, sd, upper, lower);
}

/* Normalise by subtracting the mean and dividing by TWICE the
   standard deviation. */
void normalise(EST_Track &tr, EST_FVector &mean, EST_FVector &sd,
	       float upper, float lower)
{
    for (int i = 0; i < tr.num_channels(); ++i)
	normalise(tr, mean(i), sd(i), i, upper, lower);
}

void normalise(EST_Track &tr, float mean, float sd, int channel,
	       float upper, float lower)
{
    // This scales the data so that 2 standard deviations worth of values
    // lie between upper and lower.
    int i;
    // cout << "upper = " << upper << " lower " << lower << endl;
    for (i = 0; i < tr.num_frames(); ++i)
	if (!tr.track_break(i))
	    tr.a(i, channel) = ((((tr.a(i, channel) - mean) / (4 *sd)) + 0.5)
				* (upper -lower))  + lower; 
}

EST_Track differentiate(EST_Track &c, float samp_int)
{
    // Differentiate track. SEE ALSO delta(EST_Track, int) which does
    // this in a more sophisticated way!!!
    
    EST_Track diff;
    int i, j;
    float dist;
    
    if (samp_int != 0.0)
	c.sample(samp_int);
    
    diff.copy_setup(c);
    diff.resize(c.num_frames() - 1, c.num_channels());
    
    for (i = 0; i < diff.num_frames(); ++i)
    {
	dist = c.t(i + 1) - c.t(i);
	for (j = 0; j < diff.num_channels(); ++j)
	    diff.a(i, j) = (c.track_break(i) || c.track_break(i + 1)) ? 0.0 
		: (c.a(i + 1) - c.a(i)) / dist;
	diff.t(i) = c.t(i) + (dist / 2.0);
    }
    
    return diff;
}

EST_Track difference(EST_Track &a, EST_Track &b)
{
    int i, j;
    
    int size = Lof(a.num_frames(), b.num_frames());
    EST_Track diff = a;
    
    // ERROR REORG - this needs to return a proper error
    if (a.num_channels() != b.num_channels())
    {
	cerr << "Error: Can't compare " << a.num_channels() << 
	    " channel EST_Track with " << b.num_channels() << " channel EST_Track\n";
	return diff;
    }
    
    for (i = 0; i < size; ++i)
	for (j = 0; j < a.num_channels(); ++j)
	    diff.a(i, j) = a.a(i, j) - b.a(i, j);
    
    return diff;
}

EST_Track difference(EST_Track &a, EST_Track &b, int channel_a, int channel_b)
{
    int i;
    
    int size = Lof(a.num_frames(), b.num_frames());
    EST_Track diff = a;
    
    for (i = 0; i < size; ++i)
	diff.a(i, channel_a) = a.a(i, channel_a) - b.a(i, channel_b);
    
    return diff;
}

EST_Track difference(EST_Track &a, EST_Track &b, EST_String fname)
{
    int ch_a, ch_b;
    EST_Track cor;

    if (!a.has_channel(fname))
    {
	cerr << "Error: Couldn't find field named " << fname <<
	    " in first Track\n";
	return cor;
    }

    if (!b.has_channel(fname))
    {
	cerr << "Error: Couldn't find field named " << fname <<
	    " in second Track\n";
	return cor;
    }

    ch_a = a.channel_position(fname);
    ch_b = b.channel_position(fname);

    return difference(a, b, ch_a, ch_b);
}


float mean( const EST_Track &tr, int channel )
{
  if ( channel<0 || channel >= tr.num_channels() )
    EST_error( "Tried to access channel %d of %d channel track", 
	       channel, tr.num_channels() );

  float mean=0.0;
  int i, n;
  int tr_num_frames = tr.num_frames();

  for( i=0, n=0; i<tr_num_frames; ++i )
    if( !tr.track_break(i) ){
      mean += tr.a_no_check( i, channel );
      ++n;
    }

  return mean/(float)n;
}

void mean( const EST_Track &tr, EST_FVector &m )
{
  unsigned int tr_num_channels = tr.num_channels();

  m.resize( tr_num_channels, 0 );
  
  for( unsigned int i=0; i<tr_num_channels; ++i )
    m.a_no_check(i) = mean( tr, i );
}


/** Calculate the mead and standard deviation for a single channel of a track
*/

void meansd(EST_Track &tr, float &m, float &sd, int channel)
{
  int i, n;

  m = mean( tr, channel );

  float var=0.0;
  int tr_num_frames = tr.num_frames();
  for( i=0, n=0; i<tr_num_frames; ++i)
    if( !tr.track_break(i) ){
      var += pow(tr.a_no_check(i, channel) - m, float(2.0));
      ++n;
    }

  if( n>1 ){ // use n, not tr_num_frames because of breaks
    var /= (float) (n-1);
    sd = sqrt(var);
  }
  else
    sd = 0.0;
}

/** Calculate the root mean square error between the same channel in
two tracks 
@see abs_error, rms_error
*/
float rms_error(EST_Track &a, EST_Track &b, int channel)
{
    int i;
    int size = Lof(a.num_frames(), b.num_frames());
    float sum = 0;
    
    for (i = 0; i < size; ++i)
	if (a.val(i) && b.val(i))
	  sum += pow((a.a(i, channel) - b.a(i, channel)), float(2.0));
    
    sum = sqrt(sum / size);
    return sum;
}

float abs_error(EST_Track &a, EST_Track &b, int channel)
{
    int i;
    int size = Lof(a.num_frames(), b.num_frames());
    float sum = 0;
    for (i = 0; i < size; ++i)
    {
	// cout << i << " " << a.a(i, channel) << " " << b.a(i, channel) << endl;
	if (a.val(i) && b.val(i))
	    sum += fabs(a.a(i, channel) - b.a(i, channel));
    }
    return sum / size;
}

float correlation(EST_Track &a, EST_Track &b, int channela, int channelb)
{
    int i;
    int size = Lof(a.num_frames(), b.num_frames());
    float predict,real;
    EST_SuffStats x,y,xx,yy,xy,se,e;
    float cor,error;
    
    for (i = 0; i < size; ++i)
	if (a.val(i) && b.val(i))
	{
//	    cout << a.t(i) << " " << a.a(i, channela) << " " << b.a(i, channelb) << endl;
	    predict = b.a(i, channelb);
	    real = a.a(i, channela);
	    x += predict;
	    y += real;
	    error = predict-real;
	    se += error*error;
	    e += fabs(error);
	    xx += predict*predict;
	    yy += real*real;
	    xy += predict*real;
	}
    
    cor = (xy.mean() - (x.mean()*y.mean()))/
        (sqrt(xx.mean()-(x.mean()*x.mean())) *
	 sqrt(yy.mean()-(y.mean()*y.mean())));
    
//    cout << xy.mean()  << " " << x.mean() << " " << y.mean() << " "
//        << xx.mean() << " " << yy.mean() << endl;
    
    cout << "RMSE " << sqrt(se.mean()) << " Correlation is " << cor 
        << " Mean (abs) Error " << e.mean() << " (" << e.stddev() << ")" 
            << endl;
    return cor;
}

void meansd(EST_Track &a, EST_FVector &m, EST_FVector &sd)
{
    int i;
    
    m.resize(a.num_channels());
    sd.resize(a.num_channels());
    
    for (i = 0; i < a.num_channels(); ++i)
	meansd(a, m[i], sd[i], i);
}

void meansd(EST_TrackList &tl, float &mean, float &sd, int channel)
{
    EST_Litem *p;
    float var=0.0;
    int i, n;

    n = 0;
    mean = 0.0;

    for (p = tl.head(); p; p = p->next())
	for (i = 0; i < tl(p).num_frames(); ++i)
	{
	    if (!tl(p).track_break(i))
	    {
		mean += tl(p).a(i, channel);
		++n;
	    }
    }

    mean /= n;

    for (p = tl.head(); p; p = p->next())
	for (i = 0; i < tl(p).num_frames(); ++i)
	    if (!tl(p).track_break(i))
	      var +=  pow(tl(p).a(i, channel) - mean, float(2.0));

    var /= n;
    sd = sqrt(var);
}

void meansd(EST_TrackList &tl, EST_FVector &m, EST_FVector &sd)
{
    int i;
    
    m.resize(tl.first().num_channels());
    sd.resize(tl.first().num_channels());

    for (i = 0; i < tl.first().num_channels(); ++i)
	meansd(tl, m[i], sd[i], i);
}

EST_FVector rms_error(EST_Track &a, EST_Track &b)
{
    int i;
    EST_FVector e;
    
    // ERROR REORG - this needs to return a proper error
    if (a.num_channels() != b.num_channels())
    {
	cerr << "Error: Can't compare " << a.num_channels() << 
	    " channel EST_Track with " << b.num_channels() << " channel EST_Track\n";
	return e;
    }
    e.resize(a.num_channels());
    for (i = 0; i < a.num_channels(); ++i)
	e[i] = rms_error(a, b, i);
    
    return e;
}

EST_FVector abs_error(EST_Track &a, EST_Track &b)
{
    int i;
    EST_FVector e;
    
    // ERROR REORG - this needs to return a proper error
    if (a.num_channels() != b.num_channels())
    {
	cerr << "Error: Can't compare " << a.num_channels() << 
	    " channel EST_Track with " << b.num_channels() << " channel EST_Track\n";
	return e;
    }
    e.resize(a.num_channels());
    for (i = 0; i < a.num_channels(); ++i)
	e[i] = abs_error(a, b, i);
    
    return e;
}

EST_FVector correlation(EST_Track &a, EST_Track &b)
{
    int i;
    EST_FVector cor;
    
    // ERROR REORG - this needs to return a proper error
    if (a.num_channels() != b.num_channels())
    {
	cerr << "Error: Can't compare " << a.num_channels() << 
	    " channel EST_Track with " << b.num_channels() << " channel EST_Track\n";
	return cor;
    }
    cor.resize(a.num_channels());
    for (i = 0; i < a.num_channels(); ++i)
	cor[i] = correlation(a, b, i, i);
    
    return cor;
}

EST_FVector correlation(EST_Track &a, EST_Track &b, EST_String fname)
{
    int ch_a, ch_b;
    EST_FVector cor;

    if (!a.has_channel(fname))
    {
	cerr << "Error: Couldn't find field named " << fname <<
	    " in first Track\n";
	return cor;
    }

    if (!b.has_channel(fname))
    {
	cerr << "Error: Couldn't find field named " << fname <<
	    " in second Track\n";
	return cor;
    }

    ch_a = a.channel_position(fname);
    ch_b = b.channel_position(fname);

    cor.resize(1);
    cor[0] = correlation(a, b, ch_a, ch_b);
    
    return cor;
}

EST_Track error(EST_Track &ref, EST_Track &test, int relax)
{
    int i, j, k, l;
    EST_Track diff;
    diff = ref;
    float t;
    
    // relaxation allows an error to be ignored near boundaries. The
    // degree of relation specifies how many frames can be ignored.
    
    float *r = new float[relax*3];
    
    for (l = 0; l < ref.num_channels(); ++l)
	for (i = 0; i < ref.num_frames(); ++i)
	{
	    t = 0;
	    for (k = 0, j = Gof((i - relax), 0); j < i + relax + 1; ++j, ++k)
	    {
		if (ref.a(i, l) > 0.5)
		    r[k] = ((j < test.num_frames()) && (test.a(j, l)> 0.6)) ?1 
			: 0.5;
		else
		    r[k] = ((j < test.num_frames()) && (test.a(j, l)< 0.4)) ? -1 
			: -0.5;

		// fix for relaxation
		t = r[k];
	    }
//	    cout << "ref: " << ref.a(i, l) << " test:" << test.a(i, l) << " error:" << t << endl;
	    diff.a(i, l) = t;
	}
    
    delete [] r;
    return diff;
}

void align_to_track(EST_Track &tr, float &start, float &end)
{
  int is, ie;

  // cout << " in " << start << " " << end << "\n";

  is = tr.index(start);
  ie = tr.index(end);

  // cout << " indexes " << is << " " << ie << "\n";

  start = tr.t(is);
  end   = tr.t(ie);
  // cout << " out " << start << " " << end << "\n";
}

void align_to_track(EST_Track &tr, int &start, int &end, int sample_rate)
{
    float start_t = start/(float)sample_rate;
    float   end_t =   end/(float)sample_rate;

    
    // cout << "align " << start_t << " " << end_t << " " << sample_rate << "\n";
    align_to_track(tr, start_t, end_t);
    // cout << " gives " << start_t << " " << end_t << "\n";

    start = (int)(start_t*sample_rate + 0.5);
      end = (int)(  end_t*sample_rate + 0.5);
}

void move_to_frame_ends(EST_Track &tr, 
			int &start, int &end, 
			int sample_rate,
			float offset)
{
    float start_t = start/(float)sample_rate;
    float   end_t =   end/(float)sample_rate;
    
    // cout << "move " << start_t << " " << end_t << " " << sample_rate << "\n";

    int is = tr.index(start_t-offset);
    int ie = tr.index(end_t-offset);

    int start_s, start_c, start_e;
    int end_s, end_c, end_e=0;

    if (tr.has_channel(channel_length))
    {
	get_frame(tr, sample_rate, is, start_s, start_c, start_e);
	get_frame(tr, sample_rate, ie, end_s, end_c, end_e);
    }
    else
    {
	start_s = (int)(tr.t(is) * sample_rate);
	end_s = (int)(tr.t(ie) * sample_rate);
    }
  
    start = start_s + (int)(offset*sample_rate + 0.5);
    end = end_e   + (int)(offset*sample_rate + 0.5);
}

int nearest_boundary(EST_Track &tr, float time, int sample_rate, float offset)
{
    time -= offset;

    float distance = 10000;

    for (int i = 0; i < tr.num_frames(); ++i)
    {
	float start, center, end;

	get_frame(tr, sample_rate, i, start, center, end);

	// printf("nb %f: %d distance %f start %f\n", time, i, distance, start);
	
	if (fabs(start-time) > distance)
	    return i-1;
	distance = fabs(start-time);
    }
    
    return  tr.num_frames();
}

void move_start(EST_Track &tr, float shift)
{
    for(int i=0; i<tr.num_frames(); i++)
	tr.t(i) += shift;
}

void set_start(EST_Track &tr, float start)
{
    float shift = start - tr.t(0);
    
    move_start(tr, shift);
}


void extract2(EST_Track &orig, float start, float end, EST_Track &ret)
{
    int from, to;
    int i, j;
    from = orig.index(start);
    to = orig.index_below(end);

    ret.copy_setup(orig);
    
    ret.resize(to - from, orig.num_channels());
    
    for (i = 0; i < ret.num_frames(); ++i)
	for (j = 0; j < ret.num_channels(); ++j)
	{
	    ret.a(i, j) = orig.a(i + from, j);
	    ret.t(i) = orig.t(i + from);
	    if (orig.track_break(i + from))
		ret.set_break(i);
	    else
		ret.set_value(i);
	}
    
    
    // cout << "from " << from << " to " << to << endl;
    // cout << "times from " << orig.t(from) << " to " << orig.t(to) << endl;
    
    //    orig.sub_track(ret, from, to);
    // cout << ret.num_frames() << " " << ret.start() << " " << ret.end() << endl;
    // cout << "ret " << ret;
}


void extract(EST_Track &orig, float start, float end, EST_Track &ret)
{
    int new_num_frames;
    
    ret.copy_setup(orig);
    
    int i, j;
    int is = 0, ie = 0;
    
    is = orig.index(start);
    ie = orig.index(end);
    
    // check in case above results in negative length
    new_num_frames = (ie - is) > 0 ?ie - is : 0;
    ret.resize(new_num_frames, orig.num_channels());
    
    for (i = 0; i < new_num_frames; ++i)
    {
	for (j = 0; j < orig.num_channels(); ++j)
	    ret.a(i, j) = orig.a(i + is, j);
	ret.t(i) = orig.t(i + is);
	if (orig.track_break(i + is))
	    ret.set_break(i);
	else
	    ret.set_value(i);
    }
}

int get_order(const EST_Track &t, EST_CoefficientType type, int d)
{
    int order;
    EST_ChannelType start_c = (EST_ChannelType)EST_CoefChannelId(type, d, 0);
    EST_ChannelType end_c = (EST_ChannelType)EST_CoefChannelId(type, d, 1);
    
    if (t.has_channel(start_c))
	if (t.has_channel(end_c))
	    order = t.channel_position(end_c) - t.channel_position(start_c);
	else
	    order = t.num_channels()-t.channel_position(start_c)-1;
    else
	order=0;
    return order;
}

int get_order(const EST_Track &tr)
{
    int order=0;
    EST_CoefficientType t;
    
    for(t=cot_first; t <cot_free; t=(EST_CoefficientType)(t+1))
	if ((order=get_order(tr,t))>0)
	    return order;
    
    cout << "No coefficients in track\n";
    return 0;
}

int sum_lengths(const EST_Track &t, 
		int sample_rate,
		int start_frame, int end_frame)
{
    (void)sample_rate;
    int l=0;
    
    if (end_frame < 0)
	end_frame = t.num_frames();
    
    if (t.has_channel(channel_length))
	for(int i=start_frame; i<end_frame; i++)
	    l += (int)t.a(i, channel_length);
    else
    {
	cout << "no length channel";
    }
    
    return l;
}

void get_start_positions(const EST_Track &t, int sample_rate, 
			 EST_TBuffer<int> &pos)
{
    pos.ensure(t.num_frames());
    
    if (!t.has_channel(channel_length))
    {
	cout << "no length channel\n";
	return;
    }
    
    for(int i=0; i<t.num_frames(); i++)
    {
	int wstart, wcent, wend;
	get_frame(t, sample_rate, i, wstart, wcent, wend);
	pos[i] = wstart;
	// cout << "frame " << i << " t " << t.t(i) << " sr " << sample_rate << " offset " << t.a(i,channel_offset) << " cent " << wcent << " pos " << wstart << "\n";
    }
}


void extract(EST_Track &tr, EST_Option &al)
{
    int from, to;
    EST_Track sub_track;
    
    if (al.present("-start"))
	from = tr.index(al.fval("-start"));
    else if (al.present("-from"))
	from = al.ival("-from");
    else
	from = 0;
    
    if (al.present("-end"))
	to = tr.index(al.fval("-end"));
    else if (al.present("-to"))
	to = al.ival("-to");
    else
	to = tr.num_frames() - 1;
    
    tr.sub_track(sub_track, from, to-from+1, 0, EST_ALL);
    EST_Track tr2 = sub_track;
    tr = tr2;
}

void extract_channel(EST_Track &orig, EST_Track &nt, EST_IList &ch_list)
{
    int new_ch, i, j, k;
    EST_Litem *p;
    new_ch = ch_list.length();
    
    nt.copy_setup(orig);
    nt.resize(orig.num_frames(), new_ch);
    
    for (i = 0, p = ch_list.head(); p; p = p->next(), ++i)
    {
	k = ch_list(p);

	if (k >= orig.num_channels())
	    EST_error("Tried to extract channel number %d from track with "
		      "only %d channels\n",  k, orig.num_channels());

	for (j = 0; j < orig.num_frames(); ++j)
	    nt.a(j, i) = orig.a(j, k);
	nt.set_channel_name(orig.channel_name(k), i);
    }
    for (j = 0; j < orig.num_frames(); ++j)
	nt.t(j) = orig.t(j);
}

void ParallelTracks(EST_Track &a, EST_TrackList &list,const EST_String &style)
{
    // Make multi channel track out of list of tracks. There are two
    // "styles". "0" means take the size of the first track in the list,
    // "1" means take the size of the longest as the number of frames in
    // the created track.
    EST_Litem *p, *longest;
    int num_channels, num_frames;
    int i, j, k, n;
    
    for (num_channels=0,p=list.head(); p; p=p->next())
	num_channels += list(p).num_channels();
    
    if (style == "first")
    {
	num_frames = list.first().num_frames();
	longest = list.head();
    }
    else
    {   
	if (style != "longest")
	    cerr << "EST_Track: unknown combine style \"" << style << 
		"\" assuming longest" << endl;
	for (num_frames = 0, longest = p = list.head(); p; p = p->next())
	    if (num_frames < list(p).num_frames())
	    {
		num_frames = list(p).num_frames();
		longest = p;
	    }
    }
    
    a.resize(num_frames, num_channels);
    a.fill(0.0);
    
    for (k = 0, p = list.head(); p; p = p->next())
    {
	n = Lof(num_frames, list(p).num_frames());
	for (j = 0; j < list(p).num_channels(); ++j, ++k)
	{
	    for (i = 0; i < n; ++i)
		a(i, k) = list(p).a(i, j);
	    a.set_channel_name(list(p).channel_name(j), k);
	}
    }
    // fill time with times from longest file.
    for (i = 0; i < list(longest).num_frames(); ++i)
	a.t(i) = list(longest).t(i);
}

void channel_to_time(EST_Track &tr, int channel, float scale)
{  
    
    for(int i=0; i < tr.num_frames(); i++)
    {
	tr.t(i) = tr.a(i,channel) * scale;
    }
    tr.set_equal_space(FALSE);
}

void channel_to_time(EST_Track &tr, EST_ChannelType c, float scale)
{
    int channel = NO_SUCH_CHANNEL;
    
    if (tr.map() != 0 && (channel = (tr.map()->get(c)) != NO_SUCH_CHANNEL))
    {
	channel_to_time(tr, channel, scale);
	return;
    }
    else
    {
	cerr << "no channel '" << EST_default_channel_names.name(c) << "' = " << (int)c << "\n";
	abort();
    }
}

void channel_to_time(EST_Track &tr, const EST_String c_name, float scale)
{
    for (int c=0; c<tr.num_channels(); c++)
	if (tr.channel_name(c) == c_name)
	{
	    channel_to_time(tr, c, scale);
	    return;
	}
    
    cerr << "no channel named '" << c_name << "'\n";
    abort();
}

void channel_to_time_lengths(EST_Track &tr, int channel, float scale)
{  
    float tt=0;
    for(int i=0; i < tr.num_frames(); i++)
    {
	// cout << "c_t_t " << i << " " << tt << "\n";
	tr.t(i) = tt;
	tt += tr.a(i,channel) * scale;
    }
    tr.set_equal_space(FALSE);
}

void channel_to_time_lengths(EST_Track &tr, EST_ChannelType c, float scale)
{
    int channel = NO_SUCH_CHANNEL;
    
    if (tr.map()!=0 && (channel = tr.map()->get(c)) != NO_SUCH_CHANNEL)
    {
	channel_to_time_lengths(tr, channel, scale);
	return;
    }
    else
    {
	cerr << "no channel '" << EST_default_channel_names.name(c) << "' = " << (int)c << "\n";
	abort();
    }
}

void channel_to_time_lengths(EST_Track &tr, const EST_String c_name, float scale)
{
    for (int c=0; c<tr.num_channels(); c++)
	if (tr.channel_name(c) == c_name)
	{
	    channel_to_time_lengths(tr, c, scale);
	    return;
	}
    
    cerr << "no channel named '" << c_name << "'\n";
    abort();
}

EST_String options_subtrack(void)
{
    return
	EST_String("")+
	"-start <float>   Extract track starting at this time, \n"
	"                 specified in seconds\n\n"
	"-end   <float>   Extract track ending at this time, \n"
	"                 specified in seconds\n\n"
	"-from  <int>     Extract track starting at this frame position\n\n"
	"-to    <int>     Extract track ending at this frame position\n\n";
}

EST_String options_track_input(void)
{
    // The standard waveform input options 
    return
	EST_String("")+
	"-itype <string>  Input file type (optional).  If no type is\n"
	"                 specified type is automatically derived from\n"
	"                 file's header. Supported types\n"
	"                 are: "+options_track_filetypes()+"\n\n"
// remove ???	
	"-ctype <string>  Contour type: F0, track\n\n"
	"-s <float>       Frame spacing of input in seconds, for unheadered input file\n\n"
        "-startt <float>  Time of first frame, for formats which don't provide this\n\n"
	"-c <string>      Select a subset of channels (starts from 0). \n"
	"                 Tracks can have multiple channels. This option \n"
        "                 specifies a list of numbers, refering to the channel \n"
	"                 numbers which are to be used for for processing. \n\n"+
	options_subtrack();
}


EST_String options_track_output(void)
{
    // The standard track output options 
  return
	EST_String("")+
	"-otype <string> {ascii}\n"+
	"    Output file type, if unspecified ascii is\n"+
        "    assumed, types are: "+options_track_filetypes()+", label\n\n"+
        "-S <float>       Frame spacing of output in seconds. If this is \n"
	"    different from the internal spacing, the contour is \n"
	"    resampled at this spacing \n\n"
	"-o <ofile>       Output filename, defaults to stdout\n\n";
}

void track_info(EST_Track &t)
{
    cout <<  t.name() << endl;
    cout << "Number of frames: " << t.num_frames() << endl;
    cout << "Number of channels: " << t.num_channels() << endl;
    cout << "File type: " << EST_TrackFile::map.name(t.file_type()) << endl;
    if (t.equal_space())
	cout << "Frame shift: " << t.shift() << endl;
    else
	cout << "Frame shift: varied" << endl;
    for (int i = 0; i < t.num_channels(); ++i)
	cout << "Channel: " << i << ": " << t.channel_name(i) << endl;
}

EST_String options_track_filetypes(void)
{
    // Returns list of currently support track filetypes
    // Should be extracted from the list in EST_Track
    
    return EST_TrackFile::options_short();
}

EST_String options_track_filetypes_long(void)
{
    // Returns list of currently support track filetypes
    // Should be extracted from the list in EST_Track
    
    return EST_TrackFile::options_supported();
}

