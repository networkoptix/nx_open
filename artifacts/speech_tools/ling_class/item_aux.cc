/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1998                            */
/*                        All Rights Reserved.                           */
/*                                                                       */
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
/*                                                                       */
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
/*                   Author :  Alan W Black                              */
/*                   Date   :  May 1998                                  */
/*-----------------------------------------------------------------------*/
/*  Various auxiliary item/relation functions for tree manipulation      */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include "ling_class/EST_Item.h"
#include "ling_class/EST_Item_Content.h"
#include "ling_class/EST_Relation.h"

int in_list(const EST_Item *c,const EST_Item *l)
{
    const EST_Item *i;
    
    for (i=l; i != 0; i=i->next())
	if (i == c)
	    return TRUE;
    return FALSE;
}

int in_tree(const EST_Item *c,const EST_Item *t)
{
    EST_Item *i;

    if (t == c)
	return TRUE;
    else
    {
	for (i=daughter1(t); i != 0; i=next_sibling(i))
	    if (in_tree(c,i))
		return TRUE;
	return FALSE;
    }
}

void remove_item_list(EST_Relation *rel, EST_Item *item)
{
    if (item==NULL)
	return;

    EST_Item *p = item->prev();
    EST_Item *n = item->next();

    rel->remove_item(item);

    EST_Item::splice(p,n);
}

int merge_item(EST_Item *from, EST_Item *to)
{
    // Make all references to from be references to to and merge
    // from's features into to
    EST_Item *i;

    merge_features(to->features(),from->features());

    EST_Litem *r;
    for (r = from->relations().list.head(); r; r=r->next())
    {
	i = item(from->relations().list(r).v);
	if (i != from)
	  i->set_contents(to->contents());
    }
    from->set_contents(to->contents());

    return TRUE;
}

void merge_features(EST_Item *to, EST_Item *from, int keep_id)
{
    // Merge item features, but with option of preserving ids
    EST_String keep;

    if (keep_id) keep = to->S("id", "0");
    merge_features(to->features(),from->features());
    if (keep_id) to->set("id", keep);
}

int move_item(EST_Item *from, EST_Item *to)
{
    // from's contents be to's contents, deleting from and all 
    // its daughters from to's relation. 
    EST_Item *rfrom = from->as_relation(to->relation_name());

    to->set_contents(from->contents());
    if (rfrom != 0) // from is current in this relation
	delete rfrom;  // so delete it and its daughters

    return TRUE;
}

int move_sub_tree(EST_Item *from, EST_Item *to)
{
    // make from's contents be to's contents, delete all of to's
    // daughters and rebuild from's descendants beneath to.
    EST_Item *rfrom = from->as_relation(to->relation_name());
    EST_Item *d,*r,*nr;

    if (in_tree(to,from))
	return FALSE;  // can't do that 

    to->set_contents(from->contents());
    // Remove current daughters, but don't delete them 
    // until after the copy in case from is within to's daughters
    d = to->grab_daughters();  
    if (rfrom == d)
      d = d->next();
    if ((rfrom != 0) && (daughter1(rfrom)))
    {   // copy the descendant structure
	copy_node_tree(daughter1(rfrom),to->insert_below(daughter1(rfrom)));
	delete rfrom;
    }
    for (r=d; r; r=nr)
    {
       nr = r->next();
       delete r;
    }

    return TRUE;
}

int exchange_sub_trees(EST_Item *from,EST_Item *to)
{
    // Take contents of from and its daughters and replace
    // them with contents of to and its daughters (and the reverse)
    EST_Item *rfrom = from->as_relation(to->relation_name());

    if ((!rfrom) || (in_tree(rfrom,to)) || (in_tree(to,rfrom)))
	return FALSE;  // one or other in the other
    
    EST_Item_Content *toc = to->grab_contents();
    EST_Item_Content *fromc = rfrom->grab_contents();
    EST_Item *from_d = rfrom->grab_daughters();
    EST_Item *to_d = to->grab_daughters();

    to->set_contents(fromc);
    rfrom->set_contents(toc);
    if (from_d)
	copy_node_tree(from_d,to->insert_below(from_d));
    if (to_d)
	copy_node_tree(to_d,from->insert_below(to_d));

    return TRUE;
}


EST_Item *item_jump(EST_Item *from, const EST_String &to)
{
  // This function jumps around standard festival relation structures.
  // Designed to be fast rather than anything else.
  // Behaviour is undefined for non standard structures.
  // Gives the first of non-unique items.

  int f=0,t=0;

  if (to == "Segment")
    t=1;
  else if (to == "Syllable")
    t=2;
  else if (to == "Word")
    t=3;
  else if (to == "IntEvent")
    t=4;

  if (from->in_relation("Segment"))
    f=1;
  else if (from->in_relation("Syllable"))
    f=2;
  else if (from->in_relation("Word"))
    f=3;
  else if (from->in_relation("IntEvent"))
    f=4;

  if ( t == 0 || f == 0 )
    return 0;

  if ( t == f )
    return from;

  switch(f) {

  case 1:
    // from Segment
    switch(t) {
    case 2:
      // Syllable
      return(from->as_relation("SylStructure")->up()->as_relation("Syllable"));
    case 3:
      // Word
      return(from->as_relation("SylStructure")->up()->up()->as_relation("Word"));
    case 4:
      // IntEvent
      return(from->as_relation("SylStructure")->up()->as_relation("Intonation")->down()->as_relation("IntEvent"));
    }

  case 2:
    // from Syllable
    switch(t) {
    case 1:
      // Segment
      return(from->as_relation("SylStructure")->down()->as_relation("Segment"));
    case 3:
      // Word
      return(from->as_relation("SylStructure")->up()->as_relation("Word"));
      // IntEvent
    case 4:
      return(from->as_relation("Intonation")->down()->as_relation("IntEvent"));
    }

  case 3:
    // from Word
    switch(t) {
    case 1:
      // Segment
      return(from->as_relation("SylStructure")->down()->down()->as_relation("Segment"));
    case 2:
      // Syllable
      return(from->as_relation("SylStructure")->down()->as_relation("Syllable"));
    case 4:
      return(from->as_relation("SylStructure")->down()->as_relation("Intonation")->down()->as_relation("IntEvent"));
    }

  case 4:
    // from IntEvent
    switch(t) {
    case 1:
      // Segment
      return(from->as_relation("Intonation")->up()->as_relation("SylStructure")->down()->as_relation("Segment"));
    case 2:
      // Syllable
      return(from->as_relation("Intonation")->up()->as_relation("Syllable"));
    case 3:
      // Word
      return(from->as_relation("Intonation")->up()->as_relation("SylStructure")->up()->as_relation("Word"));
    }
  }

  return NULL;
}










