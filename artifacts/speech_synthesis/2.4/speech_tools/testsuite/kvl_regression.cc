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
#include "EST_TKVL.h"
#include "EST_Option.h"
#include "EST_util_class.h"
#include "EST_types.h"

/**@name key_value_example
  * 
  * some stuff about lists
  *
  * @see EST_KVL
  * @see EST_KVI
  * @see EST_Option
  */
//@{


int main(void)
{
    EST_StrStr_KVL kvl;  // decl

    /**@name Addition
     */
    //@{

    // add item simply appends key value pairs onto the end of the list.
    // This function is useful for the initial building of a list.
      kvl.add_item("street", "South Bbridge");
      kvl.add_item("city", "Edinburgh");
      kvl.add_item("post code", "EH1 1HN");
      kvl.add_item("country", "United Kingdom");

    // by default, if a new entry has the same key name as an existing key,
    // it will not overwrite this, leaving 2 items with the same key.
    // The first will be the one accessed.
    // You can overwrite existing keys by adding a flag to this function.
    // Note  that this is much slower as all the existing keys must
    // be checked.
      kvl.add_item("country", "Scotland", 1);

    // This is equivalent to the change_item function, which is
    // used to overwrite existing entries:

      kvl.add_item("country", "Caledonia", 1);

    // Items are accessed by the val function, indexed by the key:
    // This prints the value associated with the key "country".
    cout << kvl.val("country") << endl;

    // An error is given if the key doesn't exist:
    cout << kvl.val("state") << endl;

    // This can be turned off by use of a flag. In this case the default
    // value is returned.

    cout << kvl.val("state", 0) << endl;

    // A on-the fly default value can be specified by putting using the
    // val_def function:

    cout << kvl.val_def("state", "unknown") << endl;

    // present() returns true of the key exists:
    if (kvl.present("state"))
	cout << kvl.val("state") << endl;

    //@}

}

