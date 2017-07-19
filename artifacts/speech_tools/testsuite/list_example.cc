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
#include "EST_bool.h"
#include "EST_TList.h"
#include "EST_String.h"
#include "EST_util_class.h"
#include "EST_types.h"
bool second_char_gt(const EST_UItem *uv1, const EST_UItem *uv2);

/**@name EST_TList:example
  * 
  * some stuff about lists
  *
  * @see EST_TList
  * @see EST_TKVL
  * @see EST_Option
  */
//@{

int main(void)
{

    EST_String strings[] = {"quail", "wood pigeon", "eagle", "emu", "rook" }; //decl

    // There are a number of predefined list types for EST_TList. 
    // EST_StrList is EST_TList<EST_String>.
    EST_StrList slist;  // decl
    EST_Litem *p; //decl

    /**@name Inserting items into a list

      There is no easy way to initialise a list so we'll just set it
      from the strings array.
      */

    //@{ code
    // append adds items on to the end of a list
    for (unsigned int i1 = 0; i1 < sizeof(strings) /sizeof(strings[0]); i1++)
	slist.append(strings[i1]);

    // add item to start of list
    slist.prepend("dove");

    // find pointer to "eagle", add "hawk" before it, and then add sparrow
    // after "hawk"
    for (p = slist.head(); p != 0; p = p->next())
	if (slist(p) == "eagle")
	{
	    p = slist.insert_before(p,"hawk");
	    p = slist.insert_after(p,"sparrow");
	}

    //@} code


    /**@name Iteration over a list

      A dummy pointer of type \Ref{EST_Litem} is used to iterate
      through a list. This acts somewhat like the index in an array in
      that it is used to access an item, in the list but does not
      contain a value itself.
      
      Iteration is usually done in a for loop. Initialisation involves
      setting the pointer to the head() function. Increments are done
      by the next() function. At the end of the list, the pointer will
      be set to null, and this can be used to check for the end.

      Items in the list are accessed by passing the pointer is as the
      argument to the function operator(), as in the following example.
      */
    //@{ code
    cout << "[ List Accessed by LItem\n";
    // print out contents of array.
    for (p = slist.head(); p != 0; p = p->next())
      cout << "  " << slist(p) << "\n";
    cout << "]\n";

    // items can also be accessed by their position in the list by using the
    // nth() function. The length() function returns the number of items
    // in a list.
    cout << "\n[ List Accessed by integer index\n";
    for (int i2 = 0; i2 < slist.length(); ++i2)
	cout << "  " << slist.nth(i2) << "\n";
    cout << "]\n";
    //@} code

    /**@name Accessing elements of a list

      The normal way to access an item is to use the \Ref{EST_Litem}
      in conjunction with the () operator. Other functions also exist,
      eg. first(), last() and nth(). Const and non-const version of
      each access function exist, allowing both reading and writing.
      */

    //@{ code
    // Capital;ise all 'e's in all strings
    for (p = slist.head(); p != 0; p = p->next())
	slist(p).gsub("e", "E");

    // print out last item in list
    p = slist.tail();
    cout << "Last item: " << slist(p) << endl;

    // but a more direct method is
    cout << "Last item: " << slist.last() << endl;

    // likewise with the head of the list:
    cout << "First item: " << slist.first() << endl;

    // print out the 4th item:
    cout << "4th item: " << slist.nth(4) << endl;

    // All these can be used for overwriting existing members in the list.
    // To add new members use append(), prepend(), insert_before() or 
    // insert_after() as shown in \Ref{Addition}
    
    slist.first() = "Swallow";
    slist.last() = "TurkEy";
    slist.nth(2) = "SEagull";

    //@} code  

    cout << "\n[ List After Substitutions and Replacements\n";
    for (p = slist.head(); p != 0; p = p->next())
      cout << "  " << slist(p) << "\n";
    cout << "]\n";

    /**@name Removing items from a list.
       Removing items from lists is done by having the EST_Litem point
       to a particular item, and then passing this pointer to the
       remove function. This can be tricky as this leaves the EST_Litem
       pointer pointing to a non-existent item. To get round this, the
       remove() function returns a pointer to the previous item in the
       list.
      */
    //@{ code

    // In the following example, the item "eagle" is removed and a 
    // pointer to the previous item is returned. The for loop then
    // points this to the next item in the loop, giving the appearance
    // of seamless iteration.

    for (p = slist.head(); p != 0; p = p->next())
      if (slist(p) == "EaglE")
	p = slist.remove(p);

    //@} code

    cout << "\n[ List After Removing Eagle\n";
    for (p = slist.head(); p != 0; p = p->next())
      cout << "  " << slist(p) << "\n";
    cout << "]\n";

    /**@name reverse the list.
      */

    //@{
    slist.reverse();
    //@}
    

    cout << "\n[ List After Reverse\n";
    for (p = slist.head(); p != 0; p = p->next())
      cout << "  " << slist(p) << "\n";
    cout << "]\n";

    /**@name Sorting a list
      * 
      * A number of sort routines for lists are defined. The most useful
      * are probably sort (a simple bubble sort, quick for small lists)
      * and qsort (quick-sort, faster for long lists).
      * 
      * If the default collation order is not what you want you can pass
      * a comparison operator to the sort routine.
      */

    //@{ code

    // Sort into alphabetical order
    sort(slist);
 
   cout << "\n[ Sorted\n";
   for(p=slist.head(); p ; p=p->next())
     cout << "  " << slist(p) << "\n";
   cout << "]\n";
 
    // Sort by second character.
   qsort(slist,&second_char_gt );
 
   cout << "\n[ Sorted by second character\n";
   for(p=slist.head(); p ; p=p->next())
     cout << "  " << slist(p) << "\n";
   cout << "]\n";
   //@} code
 
}

/**@name Comparison Operation Used in Sort
  * 
  * Compares the second character of Strings.
  */

//@{ code
bool second_char_gt(const EST_UItem *uv1, const EST_UItem *uv2)
{
  const EST_TItem<EST_String> *val1 = (const EST_TItem<EST_String> *)uv1;
  const EST_TItem<EST_String> *val2 = (const EST_TItem<EST_String> *)uv2;
   
  return (bool)(val1->val(1) > val2->val(1));
}
//@} code

//@}

// we would need to include the following template
// declarations if lists of strings weren't already declared.
// then this is only useful for and legal for 
// things which have < == and > defined

// template class EST_TList<EST_String>;
// template class EST_TItem<EST_String>;
// template class EST_TSortable<EST_String>;

// declare the template routines we use.

//template void sort(EST_TList<EST_String> &a,
//		   bool (*gt)(const EST_UItem *, const EST_UItem *));
//template void qsort(EST_TList<EST_String> &a,
//		   bool (*gt)(const EST_UItem *, const EST_UItem *));

