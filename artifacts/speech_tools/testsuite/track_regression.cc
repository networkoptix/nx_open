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
 /*                   Date: Fri May  9 1997                               */
 /* -------------------------------------------------------------------   */
 /* Example of declaration and use of tracks.                             */
 /*                                                                       */
 /*************************************************************************/


#include <iostream>
#include <cstdlib>
#include "EST_TrackMap.h"
#include "EST_Track.h"

void dump_track(EST_Track &tr, EST_String comment)
{
  printf("[ Track %s\n", (const char *)comment);
  for (int f=0; f<tr.num_frames(); f++)
    if (tr.val(f))
      {
	printf("  %3d:\t%3.3f", f, tr.t(f));
	for(int c=0; c<tr.num_channels(); c++)
	  printf("\t%3.3f", tr.a(f,c));
	printf("\n");
      }
    else
      printf("  BREAK\n");
  printf("]\n");
}

int main(void)

{
  EST_Track tr(20,2);
  EST_Track pm(20,0);

  // This is different on different architectures (of course)
  // cout << "EST_Track size = " << sizeof(EST_Track) << " bytes.\n";

  for(int i1=0; i1<tr.num_frames(); i1++)
    for(int j1=0; j1<tr.num_channels(); j1++)
      {
	tr.a(i1,j1) = i1 + j1/100.0;
	tr.t(i1) = i1*0.5;
	pm.t(i1) = i1*1.5;
      }

  dump_track(tr, "Original track");

  tr.rm_trailing_breaks();

  dump_track(tr, "Breaks Trimmed");

  for(int b=0; b<3; b++)
    {
      tr.set_break(b);
      tr.set_break(10+b);
      tr.set_break(tr.num_frames()-b-1);
    }
      
  dump_track(tr, "Breaks");

  tr.rm_trailing_breaks();

  dump_track(tr, "Breaks Trimmed");

  tr.resize(tr.num_frames(), tr.num_channels());

  dump_track(tr, "resized to same size");

  EST_Track st, cpy, cpy2;

  tr.sub_track(st, 3, 7, 0, EST_ALL);

  dump_track(st, "Sub Track");

  cpy = st;

  dump_track(cpy, "Copied");

  tr.copy_sub_track(cpy2 , 3, 7, 0, EST_ALL);

  dump_track(cpy2, "Copied Directly");

  dump_track(pm, "Pitch Marks");

  pm.resize(10,EST_CURRENT);

  dump_track(pm, "Resized Pitch Marks");

  return(0);
}

