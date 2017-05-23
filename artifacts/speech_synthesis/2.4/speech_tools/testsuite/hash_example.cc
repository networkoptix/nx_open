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
 /* Example of hash table use.                                           */
 /*                                                                      */
 /************************************************************************/

#include <cstdlib>
#include <iostream>
#include <cmath>
#include "EST_THash.h"
#include "EST_String.h"

// a very boring thing to do to a pair, see map below

static void look_at(EST_String &s, int &l)
{
  (void)s;
  (void)l;
  cout << ".";
}

int
main(void)
{
// Map from strings to numbers, hashed by default method.

EST_TStringHash<int> lengths(100);

lengths.add_item("fred", 4);
lengths.add_item("bill", 4);
lengths.add_item("harry", 5);

const EST_TStringHash<int> const_lengths = lengths;

// Map from ints to floats. Note, the other way around is alomst
// certainly a mistake.

EST_THash<int,float> logs(100);

logs.add_item(12, log(12.0));
logs.add_item(34, log(34.0));

cout << "length of `fred' = " << lengths.val("fred") << "\n";
cout << "log of 34' = " << logs.val(34) << "\n";

// remove items with the remove_item() method.

logs.remove_item(34);

cout << "now don't know log of 34' = " << logs.val(34) << "\n";

// the second argument to val can be used to
// ask whether the result was actually found. Useful
// when the dummy value returned for an unknown key is
// actually a valid value for the application.

int found;
float val = logs.val(123, found);

cout << "log of 123";

if (found)
  cout << " = " << val;
else
  cout << " not found";

cout << "\n";

// dump puts out a human readable version of the table for debugging
lengths.dump(cout);

// map calls a function on each pair, in this case just prints one dot per
// pair (see definition of look_at earlier).

lengths.map(look_at);
cout << "\n";

// We can step through the table with an iterator.

 EST_THash<EST_String, int>::Entries them;

 for(them.begin(const_lengths); them; them++)
   {
     cout << them->k << " " << them->v << " ";
   }

cout << "\n";

 EST_THash<EST_String, int>::RwEntries rwthem;

 for(rwthem.begin(lengths); rwthem; rwthem++)
   {
     rwthem->v *= 3;
     cout << rwthem->k << " " << rwthem->v << " ";
   }

cout << "\n";


 return 0;
}


template <> int EST_THash<int, float>::Dummy_Key = 0;
template <> float EST_THash<int, float>::Dummy_Value = 0.0;

#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_THash.cc"

Instantiate_THash(int,float)

#endif
