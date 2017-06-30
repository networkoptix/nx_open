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

int main(void)
{

  EST_Pathname f, g, abs("/tmp");

  f = EST_Pathname::construct("section", "test", "foo");

  g = "dir/subdir/";

  cout << " f = " << f << "\n";
  cout << " g = " << g << "\n";
  cout << " abs = " << abs << "\n";

  cout << " f.as_directory() = " << f.as_directory() << "\n";
  cout << " f.as_file() = " << f.as_file() << "\n";
  cout << " g.as_directory() = " << g.as_directory() << "\n";
  cout << " g.as_file() = " << g.as_file() << "\n";

  cout << " f";
  if (f.is_dirname())
    cout << " is directory\n";
  else if ( f.is_filename())
    cout << " is file\n";
  else
    cout << "is weird!\n";
  
  cout << " g";
  if (g.is_dirname())
    cout << " is directory\n";
  else if ( g.is_filename())
    cout << " is file\n";
  else
    cout << "is weird!\n";

  cout << " g";
  if (g.is_absolute())
    cout << " is absolute\n";
  else if ( g.is_relative())
    cout << " is relative\n";
  else
    cout << "is weird!\n";
  
  cout << " abs";
  if (abs.is_absolute())
    cout << " is absolute\n";
  else if ( abs.is_relative())
    cout << " is relative\n";
  else
    cout << "is weird!\n";
  
  cout << " g + f = " << g + f << "\n";
  cout << " g + abs = " << g + abs << "\n";
  cout << " abs + g + f = " << abs + (g + f) << "\n";


  return 0;
}

