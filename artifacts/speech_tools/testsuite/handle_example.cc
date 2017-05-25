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
 /*                   Date: Wed Mar 18 1998                               */
 /*                                                                       */
 /* --------------------------------------------------------------------  */
 /* Example of using the THandle reference counted pointer type.          */
 /*                                                                       */
 /*************************************************************************/


#include <cstdlib>
#include <fstream>
#include <iostream>
#include "EST_Handleable.h"
#include "EST_THandle.h"
#include "EST_TBox.h"
#include "EST_String.h"

/**@name EST_THandle:example
  * 
  * Example of using the THandle reference counted pointer type.
  *
  * @see EST_THandle
  */
//@{

/** A simple object which can be handled and reference counted.
  */

class HandleableThing : public EST_Handleable
{
private:
  EST_String p_name;

public:
  HandleableThing(EST_String name) 
    { 
      p_name=name;
      start_refcounting();
      cout << "[create-" << name << "]\n";
    }

  ~HandleableThing(void) 
    { cout << "[destroy-" << p_name << "]\n"; }

  EST_String name(void) const { return p_name; }

  friend ostream& operator << (ostream &st, const HandleableThing &t);

  HandleableThing *object_ptr() { return this; }
  const HandleableThing *object_ptr() const { return this; }
};

ostream &operator << (ostream &st, const HandleableThing &t)
{
  return st << "<<" << (const char *)t.name() << "/" << t.refcount() << ">>";
}

typedef EST_THandle<HandleableThing,HandleableThing> HandleableThingP; // decl

/** A simple object which doesn't understand reference counting.
  */

class Thing 
{
private:
  EST_String p_name;

public:
  Thing(EST_String name) 
    { 
      p_name=name;
      cout << "[create-" << name << "]\n";
    }

  ~Thing(void) 
    { cout << "[destroy-" << p_name << "]\n"; }

  EST_String name(void) const { return p_name; }

  friend ostream& operator << (ostream &st, const EST_TBox<Thing> &t);
  friend ostream& operator << (ostream &st, const Thing &t);

  Thing *object_ptr() { return this; }
  const Thing *object_ptr() const { return this; }
};

ostream &operator << (ostream &st, const EST_TBox<Thing> &t)
{
  return st << "<<[[" << t.c()->name() << "/" << t.refcount() << "]]>>";
}
  
ostream &operator << (ostream &st, const Thing &t)
{
  return st << "{" << t.name() << "}";
}
  
typedef EST_TBox<Thing> BoxedThing; // decl
typedef EST_THandle<BoxedThing,Thing> BoxedThingP; // decl

void unboxed(void)
{
  cout << "\n\nUnboxed Examples\n";
  HandleableThingP pa;
  HandleableThingP pb;

  pa = new HandleableThing("apple");
  pb = new HandleableThing("banana");
  HandleableThingP pc = new HandleableThing("cherry");

  cout << *pa
       << " " << *pb
       << "\n";

  pc=pa;
  
  cout << *pa
       << " " << *pb
       << "\n";

  pc = pb;

  cout << *pa
       << " " << *pb
       << "\n";

  pa = NULL;

  cout << "NULL"
       << " " << *pb
       << "\n";

  pa = new HandleableThing("pie");
  cout << *pa
       << " " << *pb
       << "\n";

  pb = new HandleableThing("split");
  pc = new HandleableThing("cheesecake");
  cout << *pa
       << " " << *pb
       << "\n";


}

void boxed(void)
{
  cout << "\n\nBoxed Examples\n";
  BoxedThingP pa;
  BoxedThingP pb;

  pa = new BoxedThing(new Thing("aubergene"));
  pb = new BoxedThing(new Thing("brocoli"));
  BoxedThingP pc = new BoxedThing(new Thing("cauliflower"));

  cout << *pa
       << " " << *pb
       << "\n";

  pc=pa;
  
  cout << *pa
       << " " << *pb
       << "\n";

  pc = pb;

  cout << *pa
       << " " << *pb
       << "\n";

  pa = NULL;

  cout << "NULL"
       << " " << *pb
       << "\n";

  pa = new BoxedThing(new Thing("pate"));
  cout << *pa
       << " " << *pb
       << "\n";

  pb = new BoxedThing(new Thing("quiche"));
  pc = new BoxedThing(new Thing("cheese"));
  cout << *pa
       << " " << *pb
       << "\n";


}

int main(void)
{
  unboxed();
  boxed();
  exit(0);
}

#ifdef INSTANTIATE_TEMPLATES
template class EST_THandle<HandleableThing,HandleableThing>;
template class EST_THandle<BoxedThing,Thing>;
template class EST_TBox<Thing>;
#endif

//@}
