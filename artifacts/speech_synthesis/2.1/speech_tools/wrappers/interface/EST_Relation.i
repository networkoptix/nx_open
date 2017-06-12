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
/* Swig interface for EST_Relation class                                 */
/*                                                                       */
/*************************************************************************/

%module EST_Relation 
%{
#include "ling_class/EST_Relation.h"
#include "ling_class/EST_Item.h"
%}

%include "EST_rw_status.i"
%include "EST_typemaps.i"
%import "EST_Item.i"

class EST_Relation
{
private:
  EST_String p_name;
  EST_Utterance *p_utt;
  EST_Item *p_head;   
  EST_Item *p_tail;
  
  static void node_tidy_up_val( int &k, EST_Val &v );
  static void node_tidy_up( int &k, EST_Item *node );

  void copy( const EST_Relation &r );
public:
  
  EST_Relation();
  EST_Relation( const char *name );
  EST_Relation( const EST_Relation &r ) { copy(r); }
  ~EST_Relation();
  
  EST_read_status load( const EST_String &filename,
		        const EST_String &type="esps" );

  EST_write_status save(const EST_String &filename, 
			const EST_String &type,
			bool evaluate_ff = false) const;


  // Features which belong to the relation rather than its items
  //    EST_Features f;
  
  // Evaluate the relation's feature functions
  //    void evaluate_features();
  
  // Evaluate the feature functions of all the items in the relation
  void evaluate_item_features();
  
  // Clear the relation of items
  void clear();
  
  // Return the EST_Utterance to which this relation belongs
  EST_Utterance *utt();
  
  // Set the EST_Utterance to which this relation belongs
  void set_utt( EST_Utterance *u );
  
  // name of the relation
  const EST_String &name() const;
  
  // Return the head (first) item of the relation
  EST_Item *head() const;
  
  // Return the root item of the relation
  EST_Item *root() const;
  
  // Return the tail (last) item of the relation
  EST_Item *tail() const;
  
  EST_Item *first() const;
  EST_Item *first_leaf() const;
  EST_Item *last() const;
  EST_Item *last_leaf() const;
  
  EST_Item *append( EST_Item *si );
  EST_Item *prepend( EST_Item *si );		
  
  // number of items in this relation
  int length() const;
  
  // return true if relation does not contain any items
  int empty() const;

  // remove EST_Item "item" from relation
  void remove_item( EST_Item *item );

  // remove all occurrences of feature "name" from relation's items
  void remove_item_feature( const EST_String &name );

#ifdef HAVEITEMLISTOUT
  %extend {
    %newobject items;
    inline void items( EST_Item **ITEMLISTOUT ){
      *ITEMLISTOUT = self->first();
    }
  }
#endif

};

//inline bool operator==(const EST_Relation &a, const EST_Relation &b);
void copy_relation(const EST_Relation &from, EST_Relation &to);




