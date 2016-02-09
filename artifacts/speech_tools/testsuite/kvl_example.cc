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

#if defined(DATAC)
#    define __STRINGIZE(X) #X
#    define DATA __STRINGIZE(DATAC)
#endif

/**@name EST_KVL:example
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
    EST_Litem *p; //decl
    EST_Option al; //decl
    EST_Option op; //decl

    /**@name KVL_Addition
     */
    //@{ code

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

      kvl.change_val("country", "Caledonia");

    //@} code

    /**@name KVL_Access
      The usual way to access the list is to pass in the name of the
      key to the {\tt val} function, which then returns the value
      associated with that key.
      */
      //@{ code

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
	cout << kvl.val("state") << endl;;

    // Normally, direct access to the list is not needed, but for
    // efficiency's sake, it is sometimes useful to be able to directly
    // access items. The {\tt list} variable contains the key/value
    // list, from this, \Ref{EST_Litem} pointers can be set to items, and
    // then used in access functions:

    for (p=kvl.head(); p != 0; p=p->next())
	 cout << kvl.val(p) << " " << kvl.key(p) << endl;

    // this can also be used to change values: the following changes the
    // value of the pair pointed to by p to "Scotland".

    kvl.change_val(p, "Scotland");

    // The name of the key can be changed similarly:

    kvl.change_key(p, "Nation");

    //@} code

    /**@name EST_Option_General
      The EST_Option class is a high level version of the EST_KVL class with
      strings for both keys and values. It is often used for lists of
      options, especially command line arguments.
      */
    //@{ code

    // load in options from file. The file is in the form of one key
    // value pair per line. The key ends at the end of the first
    // whitespace delimited token, which allows the values to have
    // spaces. Eg.
    // Country  Scotland
    // Street South Bridge
    // Number 80
    // Height 23.45

    // load in file
    op.load(DATA "/options.file");

    // All the normal EST_KVL accessing and addition functions
    // work. Although the type of the value is a String, functions are
    // provided to allow easy casting to ints and floats.

    cout << op.val("Street") << endl;
    // print out number as an integer
    cout << op.ival("Number") << endl;
    // print out height as a float
    cout << op.fval("Height") << endl;

    // Often, one wishes to override an existing value if a new value
    // has been set. The override_val function is useful for this. In
    // the following example, the command line argument is held in the
    // {\tt al} object. A default value is put in the length field. If
    // the command line option is present, it overrides "length",
    // otherwise "length" is left unchanged:

    op.add_fitem("length", 39.78);
    op.override_fval("length", al.fval("-l", 0));

    // This is quicker than the alternative:

    op.add_fitem("length", 39.78);

    if (al.present("-l"))
	op.override_fval("length", al.fval("-l", 0));

    //@} code
}

//@}
