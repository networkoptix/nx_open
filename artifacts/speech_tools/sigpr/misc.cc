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
 /*************************************************************************/
 /*                                                                       */
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             */
 /* --------------------------------------------------------------------  */
 /* Some generally useful signal processing things.                       */
 /*                                                                       */
 /*************************************************************************/

#include "EST_Wave.h"
#include "EST_TBuffer.h"
#include "sigpr/EST_misc_sigpr.h"

#include "EST_inline_utils.h"

static void short_set(EST_Wave &to, int ch,
		       const EST_TBuffer<double> &from, 
		       double maxval)
{
  for(int i=0; i<to.num_samples(); i++)
      to.a_no_check(i,ch) = irint(from(i)/maxval*10000);
}

/* redundant - see new version in filter */
void EST_pre_emphasis(EST_Wave &signal, EST_Wave &psignal, float a)
{
    int num_samples = signal.num_samples();
    short last=0;
    double maxval=0;
    EST_TBuffer<double> fpdata(num_samples);

    for(int i=0; i<num_samples; i++)
    {
	fpdata[i] = signal.a(i) - a*last;
	last = signal.a(i);
	if (absval(fpdata[i]) > maxval)
	    maxval = absval(fpdata[i]);
    }

    psignal.resize(num_samples,1,FALSE);
    psignal.set_sample_rate(signal.sample_rate());
    short_set(psignal, 0, fpdata, maxval);
}

/* redundant - see new version in filter */
void EST_post_deemphasis(EST_Wave &signal, EST_Wave &dsignal, float a)
{
    int num_samples = signal.num_samples();
    double last=0;
    double maxval=0;
    EST_TBuffer<double> fddata(num_samples);

    for(int i=0; i<num_samples; i++)
    {
	fddata[i] = signal.a(i) + a*last;
	last = fddata[i];
	if (absval(fddata[i]) > maxval)
	    maxval = absval(fddata[i]);
    }

    dsignal.resize(num_samples,1,FALSE);
    dsignal.set_sample_rate(signal.sample_rate());
    short_set(dsignal, 0, fddata, maxval);
}



