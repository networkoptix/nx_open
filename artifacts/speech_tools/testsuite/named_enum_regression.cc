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
 /* Regression test for named enum type.                                 */
 /*                                                                      */
 /************************************************************************/

#include <iostream>
#include "EST_TNamedEnum.h"
#include "EST_String.h"

// #if defined(__GNUC__)
// #    define InfoType EST_String
// #else
#    define InfoType const char *
// #endif

typedef enum { c_red=1, c_blue=2, c_green=3, c_unknown=666} Colour;

Start_TNamedEnumI(Colour, InfoType, ColourMap) 
  { c_unknown, {"grey"}, "Xenon"},
  { c_red, {"red", "scarlet"}, "Mercury"},
  { c_blue, {"blue", "navy", "sad"}, "Steel"},
  { c_unknown, {"UNKNOWN COLOUR"}, "x"}
End_TNamedEnumI(Colour, InfoType, ColourMap)

typedef void (*PrintFn)(void);

void print_q(void) { cout << "???\n"; }
void print_1(void) { cout << "111\n"; }
void print_2(void) { cout << "222\n"; }
void print_3(void) { cout << "333\n"; }


Start_TValuedEnum(Colour, PrintFn, FnColourMap)
  { c_unknown, {print_q}},
  { c_red, {print_1, print_3}},
  { c_blue, {print_2}},
  { c_unknown, {NULL}}
End_TValuedEnum(Colour, PrintFn, FnColourMap)

int main(void)
{
  Colour c1 = c_red;
  Colour c2 = c_green;
  Colour c3 = c_blue;
  const char *n;

  n = ColourMap.name(c1);
  cout << "c1 is " << (n?n:"[NULL]") << " " << (n?EST_String(ColourMap.info(c1)):EST_String("[NULL]")) << "\n";

  n = ColourMap.name(c2);
  cout << "c2 is " << (n?n:"[NULL]") << "\n";

  n = ColourMap.name(c3);
  cout << "c3 is " << (n?n:"[NULL]") << " " << (n?EST_String(ColourMap.info(c3)):EST_String("[NULL]")) << "\n";

  PrintFn fn;

  cout << "print_3 ";
  if ((fn = FnColourMap.value(FnColourMap.token(print_3))))
    (*fn)();
  else
    cout << "---\n";

  cout << "print_2 ";
  if ((fn = FnColourMap.value(FnColourMap.token(print_2))))
    (*fn)();
  else
    cout << "---\n";

  cout << "c1 ";
  if ((fn = FnColourMap.value(c1)))
      (*fn)();
  else
    cout << "---\n";

  cout << "c2 ";
  if ((fn = FnColourMap.value(c2)))
      (*fn)();
  else
    cout << "---\n";

  cout << "c_unknown ";
  if ((fn = FnColourMap.value(c_unknown)))
      (*fn)();
  else
    cout << "---\n";

  exit(0);
}


#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_TNamedEnum.cc"

Instantiate_TNamedEnumI(Colour, InfoType)

Instantiate_TValuedEnum(Colour, PrintFn)


#endif
