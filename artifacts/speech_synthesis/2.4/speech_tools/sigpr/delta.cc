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
/*                     Author :  Simon King                              */
/*                     Date   :  July 1995                               */
/*-----------------------------------------------------------------------*/
/*                 Compute delta coefficients for                        */
/*                        Tracks and Tracks                              */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include "EST_Track.h"
#include "EST_Wave.h"
# define MAX_DELTA_ORDER 2
/// max. number of points on which the delta co-eff is based
# define MAX_REGRESSION_LENGTH 4

static float compute_gradient(const EST_FVector &x, int num_points);

void delta(EST_Track &tr, EST_Track &d, int regression_length)
{
    int reg_index, this_index;
    
    // need at least two points to compute gradient
    if ((regression_length < 2)||(regression_length > MAX_REGRESSION_LENGTH)){
	cerr << "delta(EST_Track&, int) : ERROR : regression_length is "
	    << regression_length << endl;
	exit(0);
    }
    
    // temp stores the points passed to compute_gradient
    EST_FVector temp(regression_length);
    
    for (int j = 0; j < tr.num_channels(); j++ )
	for (int i = 0; i < tr.num_frames(); i++)
	{
	    // copy values needed to compute gradient into temp[]
	    for (reg_index=0; reg_index<regression_length; reg_index++)
	    {
		// gradient is computed from points to left of current time
		// rather than points centred around the current time
		this_index = i - reg_index;
		if (this_index >= 0)
		    temp[reg_index] = tr.a(this_index, j);
	    }
	    
	    // gradient at frame 0 is defined as 0
	    if (i < 1)
		d.a(i, j) = 0.0;
	    else if (i < regression_length - 1)
		// enough data, but would prefer more
		// number of points available is only i+1
		d.a(i, j) = compute_gradient(temp, i + 1);
	    else
		// plenty of data, use the last regression_length points
		d.a(i, j) = compute_gradient(temp, regression_length);
	}
}

void delta(EST_Wave &tr, EST_Wave &d, int regression_length)
{
    int reg_index, this_index;
    
    // need at least two points to compute gradient
    if ((regression_length < 2)||(regression_length > MAX_REGRESSION_LENGTH)){
	cerr << "delta(EST_Track&, int) : ERROR : regression_length is "
	    << regression_length << endl;
	exit(0);
    }
    
    // temp stores the points passed to compute_gradient
    EST_FVector temp(regression_length);
    
    for (int j = 0; j < tr.num_channels(); j++ )
	for (int i = 0; i < tr.num_samples(); i++)
	{
	    // copy values needed to compute gradient into temp[]
	    for (reg_index=0; reg_index<regression_length; reg_index++)
	    {
		// gradient is computed from points to left of current time
		// rather than points centred around the current time
		this_index = i - reg_index;
		if (this_index >= 0)
		    temp.a_no_check(reg_index) = (float)tr.a(this_index, j);
	    }
	    
	    // gradient at frame 0 is defined as 0
	    if (i < 1)
		d.a(i, j) = 0;
	    else if (i < regression_length - 1)
		// enough data, but would prefer more
		// number of points available is only i+1
		d.a(i, j) = (short)compute_gradient(temp, i + 1);
	    else
		// plenty of data, use the last regression_length points
		d.a(i, j) = (short)compute_gradient(temp, regression_length);
	}
}

static float compute_gradient(const EST_FVector &x, int num_points)
{
    float gradient;
    
    // NB x[0] is the point LATEST in time
    // so x[1] is really x[t-1]
    
    // time between points is assumed to be one unit
    
    // These are solutions to least-squares fit of straight line
    // to num_points points.
    switch (num_points){
	
    case 1:
	gradient = 0.0;
	break;
	
    case 2:
	gradient = x(0) - x(1);
	break;
	
    case 3:
	gradient = (x(0) -x(2)) / 2.0;
	break;
	
    case 4:
	gradient = ( (3.0*x(0)) + x(1) - x(2) - (3.0 * x(3)) ) / 10.0;
	break;
	
    default:
	
	cerr << "compute_gradient(float*, int) : ERROR : num_points is" 
	    << num_points << endl;
	
	exit(0);
	break;
    }
    
    return gradient;
}

