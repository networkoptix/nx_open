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
 /* Instantiate vector of double matrices.                               */
 /*                                                                      */
 /************************************************************************/

#include "EST_types.h"
#include "EST_TVector.h"

static const EST_DMatrix def_val_DMatrix;
static EST_DMatrix error_return_DMatrix;
template <> const EST_DMatrix *EST_TVector<EST_DMatrix>::def_val = &def_val_DMatrix;
template <> EST_DMatrix *EST_TVector<EST_DMatrix>::error_return = &error_return_DMatrix;

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TVector.cc"

template class EST_TVector<EST_DMatrix>;

#endif

int operator !=(const EST_DMatrix &fm1, 
		const EST_DMatrix &fm2)
{
    int i,j;
    if(fm1.num_rows() != fm2.num_rows() ||
       fm1.num_columns() != fm2.num_columns() )
	return FALSE;

    for(i=0;i<fm1.num_rows();i++)
	for(j=0;j<fm1.num_columns();j++)
	    if(fm1.a_no_check(i,j) != fm1.a_no_check(i,j))
		return FALSE;
    
    return TRUE;
}


