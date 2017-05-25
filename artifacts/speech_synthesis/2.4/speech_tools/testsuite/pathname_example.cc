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
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)            */
 /*                   Date: Tue Apr 29 1997                              */
 /************************************************************************/
 /*                                                                      */
 /* Test of EST_Pathname class.                                          */
 /*                                                                      */
 /************************************************************************/

#include <cstdlib>
#include "EST_Pathname.h"

#if defined(DATAC)
#    define __STRINGIZE(X) #X
#    define DATA __STRINGIZE(DATAC)
#endif

int main(void)
{

  // a relative pathname

  EST_Pathname f;

  f= "baz/ptooie";

  // can be used as a file or a directory

  cout << "file f = " << f.as_file() << "\n";
  cout << "dir f  = " << f.as_directory() << "\n";

  // and absolute one
  
  EST_Pathname d;

  d = "/foo/bar/";

  // we can combine paths as expected

  cout << "combine = " << d+f << "\n";
  cout << "combine = " << f+f << "\n";

  // and can build up paths from components

  EST_Pathname built(EST_Pathname::construct(d,f,"test"));

  cout << "    build = " << built << "\n";
  cout << " filename = " << built.filename() << "\n";
  cout << "extension = " << built.extension() << "\n";
  cout << " basename = " << built.basename() << "\n";
  cout << "basename1 = " << built.basename(1) << "\n";

  EST_Pathname data(DATA);

  // getting the contents of a directory...
  EST_TList<EST_String> contents(data.entries());

  sort(contents);

  cout << "listing " << data.filename() << ":\n";

  EST_Litem *item;
  for(item=contents.head(); item; item=item->next())
    if (EST_Pathname(contents(item)).is_filename())
      cout << "    " << contents(item) << "\n";

  cout << "done.\n";

  return (0);
}

