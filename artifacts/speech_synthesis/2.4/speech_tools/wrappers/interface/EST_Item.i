/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                 (University of Edinburgh, UK) and                     */
/*                           Korin Richmond                              */
/*                         Copyright (c) 2003                            */
/*                         All Rights Reserved.                          */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*                                                                       */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                       Author :  Korin Richmond                        */
/*                       Date   :  14 Apr 2003                           */
/* --------------------------------------------------------------------- */
/*                                                                       */
/* Swig interface for EST_Item                                           */
/*                                                                       */
/*************************************************************************/

%module EST_Item

%{
#include "ling_class/EST_Item.h"
#include "ling_class/EST_item_aux.h"
%}

%include "EST_Features.i"
%include "EST_typemaps.i"
%include "EST_error.i"

class EST_Item 
{
private:
  EST_Item_Content *p_contents;
  EST_Relation *p_relation;
  EST_Item *n;
  EST_Item *p;
  EST_Item *u;
  EST_Item *d;
  
  void unref_contents();
  void ref_contents();
  void copy(const EST_Item &s);
  
public:
  // Default constructor
  EST_Item();
  // Copy constructor only makes reference to contents 
  EST_Item(const EST_Item &item);
  // Includes reference to relation 
  EST_Item(EST_Relation *rel);
  // Most common form of construction
  EST_Item(EST_Relation *rel, EST_Item *si);
  // Deletes it and references to it in its contents
  ~EST_Item();
  
  //////////////////////////////////////////////////////////////
  //
  %exception { CATCH_EST_ERROR }

  // return the value of the feature "name" cast as a float
  const float F(const EST_String &name) const;
  
  // return the value of the feature name cast as a EST_String
  const EST_String S(const EST_String &name) const;
  
  // return the value of the feature name cast as a int
  const int I(const EST_String &name) const;
  
  // return the value of the feature name cast as an EST_Features
  EST_Features &A(const EST_String &name) const;

  // set feature "name" to some value */
  // (note these functions implicitly assume the compiler will
  //  create a temporary EST_String from the "name" argument
  //  when calling the respective C++ functions - it's done like
  //  that because of a conflict between SWIG typemap conversions 
  //  and handling of overloaded functions)
  void set(const char *name, int ival);
  void set(const char *name, double fval);
  void set(const char *name, const char *cval);
  void set(const char *name, EST_Features &f);

  // set feature "name" to "val", a registered feature function
  void set_function(EST_String &name, EST_String &funcname);

  // remove feature "name"
  void f_remove(const EST_String &name);
  
  %exception;
  //
  //////////////////////////////////////////////////////////////


  // find all the attributes whose values are functions, and
  // replace them with their evaluation.
  void evaluate_features();
  
  // TRUE if feature is present, FALSE otherwise */
  int f_present(const EST_String &name) const;

  // Number of items (including this) until no next item.
  int length() const;

  //const EST_Val f(const EST_String &name) const;
  //const EST_Val f(const EST_String &name, const EST_Val &def) const;
  
  // View item from another relation
  EST_Item *as_relation(const char *relname) const;

  // TRUE if this item is in named relation
  int in_relation(const EST_String &relname) const;

  // The relation name of this particular item
  const EST_String &relation_name() const;

  // The relation of this particular item
  EST_Relation *relation(void) const;
  
  // True if "li" is the same item ignoring its relation viewpoint
  int same_item(const EST_Item *li) const;

  //EST_Item *up() const;

  // Delete this item and all its occurences in other relations
  void unref_all();
  
  %extend {
    EST_Item *prepend_daughter( EST_Item *p ){
      return prepend_daughter( self, p );
    }

    EST_Item *append_daughter( EST_Item *p ){
      return append_daughter( self, p );
    }

    EST_Item* daughter1(){
      return daughter1( self );
    }
    
    EST_Item* daughtern() {
      return daughtern( self );
    }
    
    EST_Item* next_sibling(){
      return next_sibling( self );
    }
    
    EST_Item* prev_sibling(){
      return prev_sibling( self );
    }
    
    EST_Item* parent(){
      return parent( self );
    }
  }
  
  ///////////////////////////////////////////////////////////////////////////////
  // warning: entirely obfuscated macro magic and abuse ahead 
  //                                        - turn back now to preserve sanity...

  EST_Features &features();

// #ifdef HAVEITEMFEATURESFUNCTION
//   %apply (bool FEATURESOUTEVAL) { (bool b) };

//   %extend {
//     void features( bool b ){
//       //dummy function
//     }
//   }

//   %clear ( bool b);
// #endif //HAVEITEMFEATURESFUNCTION

#ifdef HAVEITEMLEAFLISTOUT
  %extend {
    %newobject leafs;
    void leafs( EST_Item **ITEMLEAFLISTOUT ){
      *ITEMLEAFLISTOUT = self;
    }
  }
#endif //HAVEITEMLEAFLISTOUT
  
#ifdef HAVEITEMDAUGHTERLISTOUT
  %extend {
    %newobject daughters;
    void daughters( EST_Item **ITEMDAUGHTERLISTOUT ){
      *ITEMDAUGHTERLISTOUT = self;
    }
  }
#endif
};


//inline EST_Item *as(const EST_Item *n,const char *relname); //as is now keyword in python
inline EST_Item *next_item(const EST_Item *node);
inline EST_Item *first_leaf(const EST_Item *n);
inline EST_Item *last_leaf(const EST_Item *n);
inline EST_Item *next_leaf(const EST_Item *n);
int num_leaves(const EST_Item *n);
void remove_item(EST_Item *l, const char *relname);
void copy_node_tree(EST_Item *from, EST_Item *to);
void copy_node_tree_contents(EST_Item *from, EST_Item *to);

//Rob's function for jumping around HRG structures more easily
EST_Item *item_jump(EST_Item *from, const EST_String &to);
