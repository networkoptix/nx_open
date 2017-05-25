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
/* Swig interface for EST_Utterance class                                */
/*                                                                       */
/*************************************************************************/

%module EST_Utterance
%{
#include "ling_class/EST_Utterance.h"
%}

%include "EST_typemaps.i"
%include "EST_error.i"

%import "EST_Item.i"
%import "EST_Relation.i"

// List of functions which can throw errors when called (which need to be 
// caught or the interpreter will bomb out)
%exception EST_Utterance::relation CATCH_EST_ERROR;
%exception EST_Utterance::load CATCH_EST_ERROR;
%exception EST_Utterance::id CATCH_EST_ERROR;
%exception utterance_merge CATCH_EST_ERROR;

class EST_Utterance{
private:
  int highest_id;
public:

  // constructors
  EST_Utterance();
  EST_Utterance(const EST_Utterance &u) { copy(u); }
  ~EST_Utterance() {clear(); }

  //  initialise utterance
  void init();
  
  // remove everything in utterance
  void clear();
  
  // clear the contents of the relations only
  void clear_relations();

  // set the next id to be "n"
  void set_highest_id( int n );

  // return the id of the next item
  int next_id();

  // load an utterance from file
  void load( const EST_String &filename );
  
  // save an utterance to file
  void save( const EST_String &filename, const EST_String &type="est_ascii" ) const;

  // Evaluate all feature functions
  void evaluate_all_features();
  
  // number of relations in this utterance
  int num_relations() const { return relations.length(); }
  
  // is the relation present?
  bool relation_present( const EST_String name ) const;

  // get relation
  EST_Relation *relation( const char *name, int err_on_not_found=1 );
  
  // return EST_Item whose id is "n"
  EST_Item *id( const EST_String &n );

  // create a new relation
  EST_Relation *create_relation( const EST_String &relname );

  // remove relation
  void remove_relation( const EST_String &relname );
		
  void sub_utterance( EST_Item *i );
};

int utterance_merge( EST_Utterance &utt,
		     EST_Utterance &sub_utt,
		     EST_Item *utt_root,
		     EST_Item *sub_root );

int utterance_merge( EST_Utterance &utt,
		     EST_Utterance &extra,
		     EST_String feature );

void sub_utterance( EST_Utterance &sub, EST_Item *i );

EST_Utterance *get_utt( EST_Item *s );
