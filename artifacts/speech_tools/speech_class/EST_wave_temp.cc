 /************************************************************************/
 /*                                                                      */
 /*                Centre for Speech Technology Research                 */
 /*                     University of Edinburgh, UK                      */
 /*                       Copyright (c) 1996,1997                        */
 /*                        All Rights Reserved.                          */
 /*                                                                      */
 /*  Permission is hereby granted, free of charge, to use and distribute */
 /*  this software and its documentation without restriction, including  */
 /*  without limitation the rights to use, copy, modify, merge, publish, */
 /*  distribute, sublicense, and/or sell copies of this work, and to     */
 /*  permit persons to whom this work is furnished to do so, subject to  */
 /*  the following conditions:                                           */
 /*   1. The code must retain the above copyright notice, this list of   */
 /*      conditions and the following disclaimer.                        */
 /*   2. Any modifications must be clearly marked as such.               */
 /*   3. Original authors' names are not deleted.                        */
 /*   4. The authors' names are not used to endorse or promote products  */
 /*      derived from this software without specific prior written       */
 /*      permission.                                                     */
 /*                                                                      */
 /*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK       */
 /*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING     */
 /*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT  */
 /*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE    */
 /*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES   */
 /*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN  */
 /*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,         */
 /*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF      */
 /*  THIS SOFTWARE.                                                      */
 /*                                                                      */
 /************************************************************************/
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)                        */
 /*                   Date: Tue May 27 1997                                           */
 /************************************************************************/

 /************************************************************************/
 /*                                                                      */
 /* temporary place fro some new functions.                                              */
 /*                                                                      */
 /************************************************************************/

#include <cmath>
#include <cstdlib>
#include "EST_Wave.h"
#include "EST_wave_aux.h"
#include "EST_simplestats.h"
#include "EST_cutils.h"

EST_Wave difference(EST_Wave &a, EST_Wave &b)
{
    int i, j;
    
    int size = Lof(a.num_samples(), b.num_samples());
    EST_Wave diff = a;
    
    // ERROR REORG - this needs to return a proper error
    if (a.num_channels() != b.num_channels())
    {
	cerr << "Error: Can't compare " << a.num_channels() << 
	    " channel EST_Wave with " << b.num_channels() << " channel EST_Wave\n";
	return diff;
    }
    
    for (i = 0; i < size; ++i)
	for (j = 0; j < a.num_channels(); ++j)
	    diff.a(i, j) = a.a(i, j) - b.a(i, j);
    
    return diff;
}

void meansd(EST_Wave &tr, float &mean, float &sd, int channel)
{
    float var=0.0;
    int i, n;

    for (n = 0, i = 0, mean = 0.0; i < tr.num_samples(); ++i)
	{
	    mean += tr.a(i, channel);
	    ++n;
	}
    
    mean /= n;
    
    for (i = 0, mean = 0.0; i < tr.num_samples(); ++i)
      var += pow(tr.a(i, channel) - mean, float(2.0));
    
    var /= n;
    sd = sqrt(var);
}

float rms_error(EST_Wave &a, EST_Wave &b, int channel)
{
    int i;
    int size = Lof(a.num_samples(), b.num_samples());
    float sum = 0;
    
    for (i = 0; i < size; ++i)
      sum += pow(float(a.a(i, channel) - b.a(i, channel)), float(2.0));
    
    sum = sqrt(sum / size);
    return sum;
}

float abs_error(EST_Wave &a, EST_Wave &b, int channel)
{
    int i;
    int size = Lof(a.num_samples(), b.num_samples());
    float sum = 0;
    for (i = 0; i < size; ++i)
    {
      // cout << i << " " << a.a(i, channel) << " " << b.a(i, channel) << endl;
      sum += fabs(float(a.a(i, channel) - b.a(i, channel)));
    }
    return sum / size;
}

float correlation(EST_Wave &a, EST_Wave &b, int channel)
{
    int i;
    int size = Lof(a.num_samples(), b.num_samples());
    float predict,real;
    EST_SuffStats x,y,xx,yy,xy,se,e;
    float cor,error;
    
    for (i = 0; i < size; ++i)
	{
	  // cout << a.a(i, channel) << " " << b.a(i, channel) << endl;
	    predict = b.a(i, channel);
	    real = a.a(i, channel);
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
    
    // cout << xy.mean()  << " " << x.mean() << " " << y.mean() << " " << xx.mean() << " " << yy.mean() << endl;
    
    // cout << "RMSE " << sqrt(se.mean()) << " Correlation is " << cor << " Mean (abs) Error " << e.mean() << " (" << e.stddev() << ")" << endl;

    return cor;
}

void absolute(EST_Wave &wave)
{
    int i, j;
    for (i = 0; i < wave.num_samples(); ++i)
	for (j = 0; j < wave.num_channels(); ++j)
	    wave.a(i, j) = abs(wave.a(i, j));
}

EST_FVector rms_error(EST_Wave &a, EST_Wave &b)
{
    int i;
    EST_FVector e;
    
    // ERROR REORG - this needs to return a proper error
    if (a.num_channels() != b.num_channels())
    {
	cerr << "Error: Can't compare " << a.num_channels() << 
	    " channel EST_Wave with " << b.num_channels() << " channel EST_Wave\n";
	return e;
    }
    e.resize(a.num_channels());
    for (i = 0; i < a.num_channels(); ++i)
	e[i] = rms_error(a, b, i);
    
    return e;
}

EST_FVector abs_error(EST_Wave &a, EST_Wave &b)
{
    int i;
    EST_FVector e;
    
    // ERROR REORG - this needs to return a proper error
    if (a.num_channels() != b.num_channels())
    {
	cerr << "Error: Can't compare " << a.num_channels() << 
	    " channel EST_Wave with " << b.num_channels() << " channel EST_Wave\n";
	return e;
    }
    e.resize(a.num_channels());
    for (i = 0; i < a.num_channels(); ++i)
	e[i] = abs_error(a, b, i);
    
    return e;
}

EST_FVector correlation(EST_Wave &a, EST_Wave &b)
{
    int i;
    EST_FVector cor;
    
    // ERROR REORG - this needs to return a proper error
    if (a.num_channels() != b.num_channels())
    {
	cerr << "Error: Can't compare " << a.num_channels() << 
	    " channel EST_Wave with " << b.num_channels() << " channel EST_Wave\n";
	return cor;
    }
    cor.resize(a.num_channels());
    for (i = 0; i < a.num_channels(); ++i)
	cor[i] = correlation(a, b, i);
    
    return cor;
}

EST_Wave error(EST_Wave &ref, EST_Wave &test, int relax)
{
    int i, j, k, l;
    EST_Wave diff;
    diff = ref;
    int t;
    
    // relaxation allows an error to be ignored near boundaries. The
    // degree of relation specifies how many samples can be ignored.
    
    int *r = new int[relax*3];
    
    for (l = 0; l < ref.num_channels(); ++l)
	for (i = 0; i < ref.num_samples(); ++i)
	{
	    t = 0;
	    for (k = 0, j = Gof((i - relax), 0); j < i + relax + 1; ++j, ++k)
	    {
		if (ref.a(i, l) > 0.5)
		    r[k] = ((j < test.num_samples()) && (test.a(j, l)> 0.6)) ?1 
			: 0;
		else
		    r[k] = ((j < test.num_samples()) && (test.a(j, l)< 0.4)) ?1 
			: 0;
		
		t |= r[k];
	    }
	    diff.a(i, l) = t;
	}
    
    delete [] r;
    return diff;
}

