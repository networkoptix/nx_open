 /*************************************************************************/
 /*                                                                       */
 /*                Centre for Speech Technology Research                  */
 /*                     University of Edinburgh, UK                       */
 /*                       Copyright (c) 1996,1997                         */
 /*                        All Rights Reserved.                           */
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
 /*                                                                       */
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             */
 /*                   Date: Tue Jul 22 1997                               */
 /* --------------------------------------------------------------------- */
 /* Example of list class use.                                            */
 /*                                                                       */
 /*************************************************************************/

#include <cstdlib>
#include <iostream>
#include "EST_TDeque.h"
#include "EST_String.h"


/**@name EST_TDeque:example
  * 
  * Examples of stack and queue use.
  *
  * @see EST_TDeque
  */
//@{
//{ code
int main(void)
{
  EST_String strings[] 
    = { "Argyle", "Bute", "Cumbernauld", "Dundee", "Edinburgh", "Fife",
	"Glasgow", "Harris", "Iona", "Jura", "Kirkwald", "Lewis",
	"Mull", "Newhaven", "Orkney", "Pitlochry", "Queensferry",
    };

  EST_TDeque<EST_String> deq(5,2);

  int i=0;

  deq.push(strings[i++]);
  deq.push(strings[i++]);
  deq.push(strings[i++]);

  cout << deq << "\n";

  cout << "0: " << deq.nth(0) << "\n";
  cout << "1: " << deq.nth(1) << "\n";

  cout << deq.pop() << "\n";
  cout << deq.pop() << "\n";

  cout << deq << "\n";

  deq.push(strings[i++]);
  deq.push(strings[i++]);
  deq.push(strings[i++]);
  deq.push(strings[i++]);
  deq.push(strings[i++]);
  deq.push(strings[i++]);

  cout << deq << "\n";

  cout << deq.back_pop() << "\n";
  cout << deq.back_pop() << "\n";
  cout << deq.back_pop() << "\n";
  cout << deq.back_pop() << "\n";

  cout << deq << "\n";

  deq.push(strings[i++]);
  deq.push(strings[i++]);
  deq.push(strings[i++]);
  deq.push(strings[i++]);

  cout << deq << "\n";

  cout << "0: " << deq.nth(0) << "\n";
  cout << "1: " << deq.nth(1) << "\n";
  cout << "2: " << deq.nth(2) << "\n";
  cout << "3: " << deq.nth(3) << "\n";

  deq.push(strings[i++]);
  deq.push(strings[i++]);
  deq.push(strings[i++]);
  deq.push(strings[i++]);

  cout << deq << "\n";

  deq.clear();
  i=0;
  deq.push(strings[i++]);
  deq.push(strings[i++]);
  cout << deq << "\n";

  deq.back_push(strings[i++]);
  deq.back_push(strings[i++]);
  cout << deq << "\n";

}
//@} code

//@}

// we would need to include the following template
// declarations if deqs of strings weren't already declared

// Declare_TDEQ_Class(EST_String, "FILLER")

// #if defined(INSTANTIATE_TEMPLATES)

// #include "../base_class/EST_TDeque.cc"

// Instantiate_TDEQ(EST_String)

// #endif


