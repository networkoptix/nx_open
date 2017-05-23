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
 /*                   Date: Wed Mar 25 1998                               */
 /* --------------------------------------------------------------------  */
 /* Instantiate the templates for time index on tracks.                   */
 /*                                                                       */
 /*************************************************************************/

#include "EST_TTimeIndex.h"
#include "EST_Track.h"

static EST_TTI_Entry<EST_Track> def_val_s;
static EST_TTI_Entry<EST_Track> error_return_s;

const EST_TTI_Entry<EST_Track> *EST_TVector<EST_TTI_Entry<EST_Track> >::def_val(&def_val_s);
EST_TTI_Entry<EST_Track> *EST_TVector<EST_TTI_Entry<EST_Track> >::error_return(&error_return_s);

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TTimeIndex.cc"
#include "../base_class/EST_TVector.cc"

template class EST_TTimeIndex<EST_Track>;
template class EST_TTI_Entry<EST_Track>;
template class EST_TVector<EST_TTI_Entry<EST_Track> >;
template int operator !=(const EST_TTI_Entry<EST_Track> &e1, 
			 const EST_TTI_Entry<EST_Track> &e2);
template ostream& operator <<(ostream &s, 
			      const EST_TTI_Entry<EST_Track> &e);
#endif

