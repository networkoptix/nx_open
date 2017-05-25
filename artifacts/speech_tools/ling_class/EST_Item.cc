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
/*  Linguistic items (e.g. words, phones etc) held as part of Relations  */
/*                                                                       */
/*  These objects may be held in relations within an utterance.  They    */
/*  fact contain two sections, a structural part and a contents part     */
/*  (EST_Item_Content) though the user will usually only see the whole   */
/*  object.  The content part may be shared between linguistic items in  */
/*  other relations, e.g. the word item may appear both in the word      */
/*  relation and the syntax relation.                                    */
/*                                                                       */
/*  Each linguistic item is in a particular relation but it is easy      */
/*  to link to that item in another relation.  Traversal of the relation */
/*  for an item in it is trivial.                                        */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include "ling_class/EST_Item.h"
#include "ling_class/EST_Relation.h"
#include "ling_class/EST_Utterance.h"
#include "EST_TKVL.h"
#include "EST_UList.h"
#include "EST_string_aux.h"

#include "ling_class_init.h"

/* Class initialisation. This is where you should register 
 * feature functions and so on.
 */
void EST_Item::class_init(void)
{
#ifdef EST_DEBUGGING
  cerr << "Calling EST_Item init\n";
#endif

  ling_class_init::use();

  EST_register_feature_functions(standard);

#ifdef EST_DEBUGGING
  cerr << "Finished EST_Item init\n";
#endif
}

EST_Item::EST_Item()
{
    p_relation = 0;
    p_contents = 0;
    n=p=u=d=0;
    set_contents(0);
}

void EST_Item::copy(const EST_Item &s)
{
    // You can't really do this in general as a node is doubly
    // linked to its neighbours and item.  Copying all the fields would
    // mean it was no longer true (unless you copied everything).
    // So all you get for this is a *copy* of the contents (not a reference
    // to)
    p_relation = 0;
    p_contents = 0;
    n=p=u=d=0;
    set_contents(0);  // get an empty contents structure
    *p_contents = *s.p_contents;
}

EST_Item::EST_Item(const EST_Item &i)
{
    copy(i);
}

EST_Item::~EST_Item()
{
    // Delete this item and its daughters (and the contents with no 
    // other links)
    // Assumes a tree structure
    EST_Item *ds,*nds;

    // Tidy up pointers to this
    if (n != 0)	
    { 
	n->p = p;
	n->u = u;  // when deleting first daughter.
    }
    if (p != 0)	p->n = n;
    if (u != 0)	u->d = n; 

    if (p_relation)
    {
	if (p_relation->p_head == this)
	    p_relation->p_head = n;
	if (p_relation->p_tail == this)
	    p_relation->p_tail = p;
    }

    // A little cleverer with the daughters
    for (ds=d; ds != 0; ds=nds)
    {
	nds=ds->n;
	delete ds;
    }

    unref_contents();
}

EST_Item::EST_Item(EST_Relation *rel)
{
    p_relation = rel;
    p_contents = 0;
    n=p=u=d=0;
}

// all internal ids are found by getting the next id number from
// the utterance and prefixing a "_" to show that this is internally
// generated.
static void assign_id(EST_Item *s)
{
    if (s->f_present("id"))
	return;

    EST_Utterance *u = get_utt(s);
    if (u != 0)
	s->set("id", "_" + itoString(u->next_id()));
}

EST_Item::EST_Item(EST_Relation *rel, EST_Item *li)
{
    p_relation = rel;
    p_contents = 0;
    n=p=u=d=0;
    set_contents(li->contents());

    assign_id(this);
}

void EST_Item::evaluate_features()
{
    evaluate(this,p_contents->f);
}

void EST_Item::unref_contents()
{
    // Unref the related contents to this item, delete if no-one else is
    // referencing it
    if (p_contents != 0)
    {
	if (p_contents->unref_relation(relation_name()))
	    delete p_contents;
	p_contents = 0;
    }
}

void EST_Item::unref_all()
{
    // Unreference this item from all its relations, deleting its contents

    p_contents->unref_and_delete();
}

const EST_String &EST_Item::relation_name() const
{ 
    return ((this == 0) || (p_relation == 0)) ? 
	EST_String::Empty : p_relation->name();
}

void EST_Item::set_contents(EST_Item_Content *new_contents)
{
    // This function is for internal use only, general use of this 
    // is likely to be unsafe.
    EST_Item_Content *c;
    if (new_contents == 0)
	c = new EST_Item_Content;
    else
	c = new_contents;

    if (p_contents != c)
    {
	unref_contents();
	p_contents = c;

	EST_Item *nn_item = p_contents->Relation(relation_name());
	if (nn_item) // this is already linked to this relation
	{   // can't recurse on set_contents
	    nn_item->p_contents = new EST_Item_Content;
	    nn_item->p_contents->relations.add_item(relation_name(),
						    est_val(nn_item));
	}
	p_contents->relations.add_item(relation_name(),est_val(this));
    }
}

int EST_Item::length() const
{
    int i=0;
    EST_Item *nn = (EST_Item *)(void *)this;
    for (; nn; nn=nn->n,i++);
    return i;
}

EST_Item *EST_Item::insert_after(EST_Item *si)
{
    // Create a new item and add it after t, and return it.
    // Include the cross link from this new item's contents to si, and
    // from si's relations fields back to the new node
    EST_Item *new_node = new EST_Item(p_relation,si);
    
    new_node->p = this;
    new_node->n = this->n;
    if (new_node->n != 0)
	new_node->n->p = new_node;
    this->n = new_node;

    if (p_relation && (p_relation->p_tail == this))
	p_relation->p_tail = new_node;

    return new_node;
}

EST_Item *EST_Item::insert_before(EST_Item *si)
{
    // Create a new node and add it before this, and return it.
    EST_Item *new_node = new EST_Item(p_relation,si);
    
    new_node->n = this;
    new_node->p = this->p;
    if (new_node->p != 0)
	new_node->p->n = new_node;
    this->p = new_node;
    // This makes an assumption that we represent trees with only
    // the first daughter pointing to the parent
    if (this->u)
    {
	new_node->u = this->u;
	new_node->u->d = new_node;
	this->u = 0;
    }

    if (p_relation && (p_relation->p_head == this))
	p_relation->p_head = new_node;

    return new_node;
}

EST_Item *EST_Item::insert_below(EST_Item *si)
{
    // Create a new node and add it below this, and return it.
    EST_Item *new_node = new EST_Item(p_relation,si);
    
    new_node->u = this;
    new_node->d = this->d;
    if (new_node->d != 0)
	new_node->d->u = new_node;
    this->d = new_node;

    return new_node;
}

EST_Item *EST_Item::insert_above(EST_Item *si)
{
    // Create a new node and add it above this, and return it.
    EST_Item *new_node = new EST_Item(p_relation,si);
    
    new_node->d = this;
    new_node->u = this->u;
    if (new_node->u != 0)
	new_node->u->d = new_node;
    this->u = new_node;

    if (p_relation && (p_relation->p_head == this))
	p_relation->p_head = new_node;
    if (p_relation && (p_relation->p_tail == this))
	p_relation->p_tail = new_node;

    return new_node;
}

EST_Item *EST_Item::insert_parent(EST_Item *si)
{
    // Insert new parent here, by added a new below node and moving
    // the contents down to it.
    if (this == 0) return 0;

    insert_below(0);
    down()->set_contents(grab_contents());
    if (si != 0)
	set_contents(si->grab_contents());
    else
	set_contents(0);

    return this;
}

EST_Item *EST_Item::last() const
{
    // To get round the const access to this
    EST_Item *node = (EST_Item *)(void *)this;

    if (this == 0) return 0;
    for (; node->n != 0; node=node->n);
    return node;
}

EST_Item *EST_Item::first() const
{
    // To get round the const access to this
    EST_Item *node = (EST_Item *)(void *)this;

    if (this == 0) return 0;
    for (; node->p != 0; node=node->p);
    return node;
}

EST_Item *EST_Item::top() const
{
    EST_Item *node = (EST_Item *)(void *)this;

    if (this == 0) return 0;
    for (; parent(node) != 0; node=parent(node));
    return node;
}

EST_Item *EST_Item::next_leaf() const
{
    if (this == 0)
	return 0;
    else if (next() != 0)
	return next()->first_leaf();
    else
	return parent(this)->next_leaf();
}

EST_Item *EST_Item::next_item() const
{
    // For traversal through a relation, in pre-order (root then daughters)
    if (this == 0)
	return 0;
    else if (down() != 0)
	return down();
    else if (next() != 0)
	return next();
    else
    {   // at the right most leaf so go up until you find a parent with a next
	for (EST_Item *pp = parent(this); pp != 0; pp = parent(pp))
	    if (pp->next())
		return pp->next();
	return 0;
    }
}

EST_Item *EST_Item::first_leaf() const
{
    // Leafs are defined as those nodes with no daughters
    if (this == 0)
	return 0;
    if (down() == 0)
	return (EST_Item *)(void *)this;
    else
	return down()->first_leaf();
}

EST_Item *EST_Item::last_leaf() const
{
    // Leafs are defined as those nodes with no daughters
    if (this == 0)
	return 0;
    else if (next())
	return next()->last_leaf();
    else if (down())
	return down()->last_leaf();
    else
	return (EST_Item *)(void *)this;
}

EST_Item *first_leaf_in_tree(const EST_Item *root)
{
    return root->first_leaf();
}

EST_Item *last_leaf_in_tree(const EST_Item *root)
{
    if (root == 0)
	return 0;
    else if (root->down() == 0)
	return (EST_Item *)(void *)root;
    else
	return root->down()->last_leaf();
}

EST_Item *EST_Item::append_daughter(EST_Item *si)
{
    if (this == 0)
	return 0;
    EST_Item *nnode;
    EST_Item *its_downs;

    // Because we don't distinguish forests properly we need
    // to do nasty things if this si is already associated to a 
    // this relation and its "in the top list"
    EST_Item *c = si->as_relation(relation_name());
    if (in_list(c,p_relation->head()))
    {
	// need to save its daughters to put on the new node
	its_downs = c->d;
	c->d = 0; // otherwise it could delete its sub tree
	if (its_downs) its_downs->u = 0;

	if (down() == 0)
	    nnode = insert_below(si);
	else
	    nnode = down()->last()->insert_after(si);
	// put daughters back on the new item
	if (its_downs)
	{
	    its_downs->u = nnode;
	    nnode->d = its_downs;
	}

	delete c;  // delete its old form from the top level
    }
    else if (down() == 0)
	nnode = insert_below(si);
    else
	nnode = down()->last()->insert_after(si);

    return nnode;
}

EST_Item *EST_Item::prepend_daughter(EST_Item *si)
{
    if (this == 0)
	return 0;
    EST_Item *nnode;
    EST_Item *its_downs;

    // Because we don't distinguish forests properly we need
    // to do nasty things if this si is already associated to a 
    // this relation and its "in the top list"
    EST_Item *c = si->as_relation(relation_name());
    if (in_list(c,p_relation->head()))
    {
	// need to save its daughters to put on the new node
	its_downs = c->d;
	c->d = 0; // otherwise it could delete its sub tree
	if (its_downs) its_downs->u = 0;

	if (down() == 0)
	    nnode = insert_below(si);
	else
	    nnode = down()->insert_before(si);
	// put daughters back on the new item
	if (its_downs)
	{
	    its_downs->u = nnode;
	    nnode->d = its_downs;
	}

	delete c;  // delete its old form from the top level
    }
    else if (down() == 0)
	nnode = insert_below(si);
    else
	nnode = down()->insert_before(si);

    return nnode;
}

EST_Item *EST_Item::grab_daughters()
{
    EST_Item *dd = down();
    if (dd)
    {
	dd->u = 0;
	d = 0;
    }
    return dd;
}

EST_Item_Content *EST_Item::grab_contents(void)
{
    // Unreference contents, but don't delete them if that's the
    // last reference.  It is the caller's responsibility to deal
    // with these contents, typically they are just about to be set
    // as contents of someone else so are only orphaned for a short
    // time
    EST_Item_Content *c = contents();
    c->unref_relation(relation_name());
    p_contents = 0;
    set_contents(0);  // can't sit without contents
    return c;
}

void copy_node_tree(EST_Item *from, EST_Item *to)
{
    // Copy this node and all its siblings and daughters

    if (from->next() != 0)
	copy_node_tree(from->next(),to->insert_after(from->next()));

    if (from->down() != 0)
	copy_node_tree(from->down(),to->insert_below(from->down()));

}

void copy_node_tree_contents(EST_Item *from, EST_Item *to)
{
    // Copy this node and all its siblings and daughters
    // also copy the item's contents

    if (from->next() != 0)
    {
	EST_Item i = *from->next();  // copies the contents
	copy_node_tree_contents(from->next(),to->insert_after(&i));
    }

    if (from->down() != 0)
    {
	EST_Item i = *from->down();
	copy_node_tree_contents(from->down(),to->insert_below(&i));
    }

}

int EST_Item::verify() const
{
    // Return FALSE if this node and its neighbours aren't
    // properly linked

    if (this == 0)
	return TRUE;
    if (((d == 0) || (d->u == this)) &&
	((n == 0) || (n->p == this)) &&
	(d->verify()) &&
	(n->verify()))
	return TRUE;
    else
	return FALSE;
}

EST_Item *append_daughter(EST_Item *n, EST_Item *p)
{ 
    return n->append_daughter(p); 
}

EST_Item *append_daughter(EST_Item *n,const char *relname, EST_Item *p)
{ 
    return append_daughter(as(n,relname),p);
}

EST_Item *prepend_daughter(EST_Item *n, EST_Item *p)
{ 
    return n->prepend_daughter(p); 
}

EST_Item *prepend_daughter(EST_Item *n, const char *relname, EST_Item *p)
{ 
    return prepend_daughter(as(n,relname),p); 
}

void remove_item(EST_Item *l, const char *relname)
{
    EST_Item *lr = l->as_relation(relname);
    EST_Relation *r = lr->relation();

    if ((lr != 0) && (r != 0))
	r->remove_item(lr);
}

EST_Item &EST_Item::operator=(const EST_Item &s)
{
    copy(s);
    return *this;
}

ostream& operator << (ostream &s, const EST_Item &a)
{
    a.features().save(s);
    return s;
}


void evaluate(EST_Item *a,EST_Features &f)
{
    EST_Features::RwEntries p;

    for(p.begin(f); p; ++p)
      if (p->v.type() == val_type_featfunc)
	{
	  if (featfunc(p->v) != NULL)
	    p->v = (featfunc(p->v))(a);
	  else
	    {
	      fprintf(stderr, "NULL %s function\n", (const char *) p->k );
	      p->v = EST_Features::feature_default_value;
	    }
	}
}

VAL_REGISTER_CLASS_NODEL(item,EST_Item)
