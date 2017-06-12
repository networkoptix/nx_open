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
 /*                                                                      */
 /*                   Date: October 1998                                 */
 /* -------------------------------------------------------------------- */
 /* Instantiate vector of double vectors (not a matrix !).               */
 /*                                                                      */
 /************************************************************************/

#include "EST_types.h"
#include "EST_TVector.h"

static const EST_DVector def_val_DVector;
static EST_DVector error_return_DVector;
template <> const EST_DVector *EST_TVector<EST_DVector>::def_val = &def_val_DVector;
template <> EST_DVector *EST_TVector<EST_DVector>::error_return = &error_return_DVector;

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TVector.cc"

template class EST_TVector<EST_DVector>;

#endif

int operator !=(const EST_DVector &fv1, 
		const EST_DVector &fv2)
{
    int i;
    if(fv1.length() != fv2.length())
	return FALSE;
    for(i=0;i<fv1.length();i++)
	if(fv1.a_no_check(i) != fv2.a_no_check(i))
	    return FALSE;

    return TRUE;
}


