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
/*                   Date   :  February 1998                             */
/*-----------------------------------------------------------------------*/
/*             Generalised relations in utterances                       */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include "ling_class/EST_Relation.h"
#include "ling_class/EST_Item.h"
#include "relation_io.h"

VAL_REGISTER_CLASS(relation,EST_Relation)

EST_Relation::EST_Relation(const EST_String &name)
{
    p_name = name;
    p_head = 0;
    p_tail = 0;
    p_utt = 0;
}

EST_Relation::EST_Relation()
{
    p_head = 0;
    p_tail = 0;
    p_utt = 0;
}

void EST_Relation::copy(const EST_Relation &r)
{
    // Do a *full* copy include the contents of all the items
    // But not the name (?)
    EST_String tmp_name;
    p_name = r.p_name;
    p_head = 0;
    p_tail = 0;
    p_utt = 0;  // can't be in the same utterance as r

    tmp_name = f.S("name", "");
    f = r.f;
    f.set("name", tmp_name);

    if (r.root() != 0)
    {
	EST_Item i = *r.root();
	EST_Item *to_root = append(&i);
	copy_node_tree_contents(r.root(),to_root);
    }
}

EST_Item *EST_Relation::append(EST_Item *si)
{

    EST_Item *nn;

    if (this == 0)
    {
      EST_warning("EST_Relation: no relation to append to");
      return 0;
    }
    else if (p_tail == 0)
    {
	nn = new EST_Item(this, si);
	p_head = nn;
    }
    else
	nn = p_tail->insert_after(si);
    p_tail = nn;

//    if (!si->f_present("id") && utt())
//	si->fset("id", utt()->next_id());

    return nn;
}

EST_Item *EST_Relation::append()
{
    return append(0);
}

EST_Item *EST_Relation::prepend()
{
    return prepend(0);
}

EST_Item *EST_Relation::prepend(EST_Item *si)
{
    EST_Item *nn;

    if (this == 0)
    {
      EST_warning("EST_Relation: no relation to prepend to");
	return 0;
    }
    else if (p_head == 0)
    {
	nn = new EST_Item(this,si);
	p_tail = nn;
    }
    else
	nn = p_head->insert_before(si);
    p_head = nn;

    return nn;
}

EST_Relation::~EST_Relation()
{
    clear();
}

int EST_Relation::length() const
{
    EST_Item *node;
    int i;

    if (this == 0)
	return 0;
    for (i=0,node=p_head; node; node=node->next())
	i++;
    return i;
}

void EST_Relation::evaluate_item_features()
{
    for (EST_Item *s = head(); s; s = s->next())
	s->evaluate_features();
}

void EST_Relation::clear()
{
    EST_Item *nn,*nnn;

    for (nn = p_head; nn != 0; nn = nnn)
    {
	nnn = nn->next();
	delete nn;
    }
    p_head = p_tail = 0;
}

void EST_Relation::remove_item(EST_Item *node)
{
    if (p_head == node)
	p_head = node->next();
    if (p_tail == node)
	p_tail = node->prev();
    delete node;
}

void EST_Relation::remove_item_feature(const EST_String &name)
{
    for (EST_Item *s = p_head; s; s = next_item(s))
	s->f_remove(name);
}

void copy_relation(const EST_Relation &from, EST_Relation &to)
{
    // clone the relation structure from into to, deleting any existing
    // nodes in to.

    to.clear();

    if (from.root() != 0)
    {
	EST_Item *to_root = to.append(from.root());
	copy_node_tree(from.root(),to_root);
    }
}

EST_write_status EST_Relation::save(ostream &outf,
				    const EST_String &type,
				    bool evaluate_ff) const
{
    if (type == "esps")
	return save_esps_label(&outf,*this,evaluate_ff);
    else if (type == "htk")
	return save_htk_label(&outf,*this);
    else
    {
      EST_warning("EST_Relation: unsupported type: \"%s\"", (const char *)type);
	return write_fail;
    }
}

EST_write_status EST_Relation::save(const EST_String &filename, 
				    const EST_String &type,
				    bool evaluate_ff) const
{
    if (type == "esps")
	return save_esps_label(filename,*this,evaluate_ff);
    else if (type == "htk")
	return save_htk_label(filename,*this);
    else
    {
      EST_warning("EST_Relation: unsupported type: \"%s\"", (const char *)type);
	return write_fail;
    }
}

EST_write_status EST_Relation::save(const EST_String &filename, 
				    bool evaluate_ff) const
{
    return save(filename,"esps",evaluate_ff);
}

EST_write_status EST_Relation::save(ostream &outf, 
				    EST_TKVL<void *,int> contents) const
{
    EST_TKVL<void *,int> nodenames;
    int node_count = 1;
    outf << "Relation " << name() << " ; ";
    f.save(outf);
    outf << endl;
    save_items(p_head,outf,contents,nodenames,node_count);
    outf << "End_of_Relation" << endl;
    return write_ok;
}

EST_write_status EST_Relation::save_items(EST_Item *node, 
					  ostream &outf,
					  EST_TKVL<void *,int> &cnames,
					  EST_TKVL<void *,int> &nodenames,
					  int &node_count) const
{
    if (node != 0)
    {
        EST_Item *n = node;
        int myname;

        while (n)
        {
            myname = node_count++;
            nodenames.add_item(n,myname);
            n = n->next();
        }

        n = node;
        while (n)
        {
            // This will need to be expanded if the we make Relations
            // have more complex structures
            save_items(n->down(),outf,cnames,nodenames,node_count);
            outf << nodenames.val(n) << " " <<
                (n->contents() == 0 ? 0 : cnames.val(n->contents())) << " " <<
                (n->up() == 0 ? 0 : nodenames.val(n->up())) << " " <<
                (n->down() == 0 ? 0 : nodenames.val(n->down())) << " " <<
                (n->next() == 0 ? 0 : nodenames.val(n->next())) << " " <<
                (n->prev() == 0 ? 0 : nodenames.val(n->prev())) << endl;
            n = n->next();
        }
    }
    return write_ok;
}

#if 0
EST_read_status EST_Relation::load(EST_TokenStream &ts,
				   const EST_THash<int,EST_Val> &contents)
{
    if (ts.get() != "Relation")
    {
	cerr << "load_relation: " << ts.pos_description() << 
	    " no new Relation" << endl;
	return misc_read_error;
    }
    p_name = ts.get().string();
    if (ts.get() != ";")
    {
	cerr << "load_relation: " << ts.pos_description() << 
	    " semicolon missing after Relation name \"" <<
		p_name << "\"" << endl;
	return misc_read_error;
    }
    if (f.load(ts) != format_ok)
	return misc_read_error;
    if (load_items(ts,contents) != format_ok)
	return misc_read_error;

    return format_ok;
}
#endif

EST_read_status EST_Relation::load(EST_TokenStream &ts,
				   const EST_TVector < EST_Item_Content * > &contents
				   )
{
    if (ts.get() != "Relation")
    {
	cerr << "load_relation: " << ts.pos_description() << 
	    " no new Relation" << endl;
	return misc_read_error;
    }
    p_name = ts.get().string();
    if (ts.get() != ";")
    {
	cerr << "load_relation: " << ts.pos_description() << 
	    " semicolon missing after Relation name \"" <<
		p_name << "\"" << endl;
	return misc_read_error;
    }
    if (f.load(ts) != format_ok)
	return misc_read_error;
    if (load_items(ts,contents) != format_ok)
	return misc_read_error;

    return format_ok;
}

void EST_Relation::node_tidy_up_val(int &k, EST_Val &v)
{
    // Called to delete the nodes in the hash table when a load
    // fails
    (void)k;
    EST_Item *node = item(v);
    node->u = 0;
    node->d = 0;
    node->n = 0;
    node->p = 0;
    delete node;
}

void EST_Relation::node_tidy_up(int &k, EST_Item *node)
{
    // Called to delete the nodes in the hash table when a load
    // fails
    (void)k;
    node->u = 0;
    node->d = 0;
    node->n = 0;
    node->p = 0;
    delete node;
}

#if 0
EST_read_status EST_Relation::load_items(EST_TokenStream &ts,
					 const EST_THash<int,EST_Val> &contents)
{
    // Load a set of nodes from a TokenStream, the file contains node
    // descriptions one per line as 5 ints, this nodes name, the
    // stream item it is to be related to, then the name of the 
    // nodes above, below, before and after it.
    EST_THash<int,EST_Val> nodenames(100);
    EST_read_status r = format_ok;
    EST_Item *node = 0;
    EST_Relation *rel=NULL;
//    int expect_links=0;

    while (ts.peek() != "End_of_Relation")
    {
	int name = atoi(ts.get().string());
	int siname;

	node = get_item_from_name(nodenames,name);
	if (!node)
	  EST_error("Unknown item %d", name);

	if (rel==NULL)
	  {
	    rel=node->relation();
//	    EST_String type = rel->f.S("type", "");
//	    expect_links = (type == "ladder");
	  }

	siname = atoi(ts.get().string());
	if (siname != 0) 
	{
	    int found;
	    EST_Val v = contents.val(siname,found);
	    if (!found)
	    {
		cerr << "load_nodes: " << ts.pos_description() << 
		    " node's item contents" << siname << " doesn't exist\n";
		r = misc_read_error;
		break;
	    }
	    else
		node->set_contents(icontent(v));
	}
	// up down next previous
	node->u = get_item_from_name(nodenames,atoi(ts.get().string()));
	node->d = get_item_from_name(nodenames,atoi(ts.get().string()));
	node->n = get_item_from_name(nodenames,atoi(ts.get().string()));
	node->p = get_item_from_name(nodenames,atoi(ts.get().string()));

	
	// Read ladder links
#if 0
	if (expect_links)
	  {
	    int numlinks = atoi(ts.get().string());
	    // node->link_feats.set("num_links",numlinks);
	    for (int i=0;i<numlinks;++i)
	      {
		EST_Item * item = get_item_from_name(nodenames,atoi(ts.get().string()));
		node->link_feats.set_val("link" + itoString(i),est_val(item));
	      }
	  }
#endif
    }
    
    ts.get(); // skip End_of_Relation

    if (r == format_ok)
    {
	if (node != 0) // at least one node
	  {
	    p_head = get_item_from_name(nodenames,1);
	    p_tail = p_head->last();
	    if (!p_head->verify())
	      {
		cerr << "load_nodes: " << ts.pos_description() <<
		  " nodes do not form consistent graph" << endl;
		r = misc_read_error;
	      }
	  }
    }

    if (r != format_ok)
    {
	// failed to read this relation so clear the created nodes
	// before returning, no idea what state the links are in so
	// explicitly unlink them before deleting them

	nodenames.map(node_tidy_up_val);
    }
    return r;
}    
#endif

EST_read_status EST_Relation::load_items(EST_TokenStream &ts,
					 const EST_TVector < EST_Item_Content * > &contents
					 )
{
    // Load a set of nodes from a TokenStream, the file contains node
    // descriptions one per line as 5 ints, this nodes name, the
    // stream item it is to be related to, then the name of the 
    // nodes above, below, before and after it.

    EST_TVector < EST_Item * > nodenames(100);
    //    EST_THash<int,EST_Val> nodenames(100);
    EST_read_status r = format_ok;
    EST_Item *node = 0;
    EST_Relation *rel=NULL;
//    int expect_links=0;

    while (ts.peek() != "End_of_Relation")
    {
	int name = atoi(ts.get().string());
	int siname;

	node = get_item_from_name(nodenames,name);
	if (!node)
	  EST_error("Unknown item %d", name);

	if (rel==NULL)
	  {
	    rel=node->relation();
//	    EST_String type = rel->f.S("type", "");
//	    expect_links = (type == "ladder");
	  }

	siname = atoi(ts.get().string());
	if (siname != 0) 
	{
	  EST_Item_Content *c = contents(siname);
	  if (c==NULL)
	    {
	      cerr << "load_nodes: " << ts.pos_description() << 
		" node's stream item " << siname << " doesn't exist\n";
	      r = misc_read_error;
	      break;
	    }
	  else
	    node->set_contents(c);
	}
	// up down next previous
	node->u = get_item_from_name(nodenames,atoi(ts.get().string()));
	node->d = get_item_from_name(nodenames,atoi(ts.get().string()));
	node->n = get_item_from_name(nodenames,atoi(ts.get().string()));
	node->p = get_item_from_name(nodenames,atoi(ts.get().string()));

#if 0
	// Read ladder links
	if (expect_links)
	  {
	    int numlinks = atoi(ts.get().string());
	    // node->link_feats.set("num_links",numlinks);
	    for (int i=0;i<numlinks;++i)
	      {
		EST_Item * item = get_item_from_name(nodenames,atoi(ts.get().string()));
		// node->link_feats.set_val("link" + itoString(i),est_val(item));
	      }
	  }
#endif
    }

    ts.get(); // skip End_of_Relation

    if (r == format_ok)
    {
	if (node != 0) // at least one node
	    p_head = get_item_from_name(nodenames,1);
	p_tail = p_head->last();
	if (!p_head->verify())
	{
	    cerr << "load_nodes: " << ts.pos_description() <<
		" nodes do not form consistent graph" << endl;
	    r = misc_read_error;
	}
    }

    if (r != format_ok)
    {
	// failed to read this relation so clear the created nodes
	// before returning, no idea what state the links are in so
	// explicitly unlink them before deleting them
      for(int ni=0; ni<nodenames.length(); ni++)
	{
	  EST_Item *node = nodenames(ni);
	  if (node != NULL)
	    node_tidy_up(ni, node);
	}
    }
    return r;
}    

EST_Item *EST_Relation::get_item_from_name(EST_THash<int,EST_Val> &nodenames,
					   int name)
{
    // Return node named by name or create a new one if it doesn't
    // already exist
    EST_Item *node;
    int found;

    if (name == 0)
      return 0;
    EST_Val v = nodenames.val(name,found);
    if (!found)
    {
	node = new EST_Item(this, 0);
	nodenames.add_item(name,est_val(node));
    }
    else
	node = item(v);
    return node;
}

EST_Item *EST_Relation::get_item_from_name(EST_TVector< EST_Item * > &nodenames,
					   int name)
{
    // Return node named by name or create a new one if it doesn't
    // already exist

    if (name == 0)
	return 0;

    if (name >= nodenames.length())
      {
	nodenames.resize(name*2, 1);
      }

    EST_Item *node = nodenames(name);
    if (node==NULL)
    {
	node = new EST_Item(this, 0);
	nodenames[name] = node;
    }

    return node;
}

EST_read_status EST_Relation::load(const EST_String &filename,
				   EST_TokenStream &ts,
				   const EST_String &type)
{
    EST_read_status r;

    f.set("filename",filename);

    if (type == "esps")
	r = load_esps_label(ts,*this);
    else if (type == "ogi")
	r = load_ogi_label(ts,*this);
    else if (type == "htk")
	r = load_sample_label(ts,*this,10000000);
    else if ((type == "ascii") || (type == "timit"))
	r = load_sample_label(ts,*this,1);
    else if (type == "words")
	r = load_words_label(ts,*this);
    else   // currently esps is the default
	r = load_esps_label(ts,*this);

    return r;
}

EST_read_status EST_Relation::load(const EST_String &filename,
				   const EST_String &type)
{
    // Load an isolated relation from a file, assuming Xlabel format
    EST_TokenStream ts;
    EST_read_status r;

    if (((filename == "-") ? ts.open(cin) : ts.open(filename)) != 0)
    {
	cerr << "load_relation: can't open relation input file " 
	    << filename << endl;
	return misc_read_error;
    }
    r = load(filename, ts, type);

    ts.close();

    return r;
}

int num_leaves(const EST_Item *h)
{
    int count = 0;
    EST_Item *n;

    for (n = h->first_leaf(); n != 0; n=n->next_leaf())
	count++;
    return count;
}

EST_Item *EST_Relation::first_leaf() const 
{ 
    return head()->first_leaf();
}

EST_Item *EST_Relation::last_leaf() const 
{ 
    return head()->last_leaf();
}

EST_Utterance *get_utt(EST_Item *s)
{
    // Occasionally you need to get the utterance from a stream_item
    // This finds any relations in s and follows them to the utterance
    // If there aren't any Relations the this streamitem isn't in an
    // utterances

    if (s == 0)
	return 0;
    if (s->relation())
	return s->relation()->utt();
    else
	return 0;  // can't find an utterance
}

EST_Relation &EST_Relation::operator=(const EST_Relation &s)
{
    copy(s);
    return *this;
}

ostream& operator << (ostream &s, const EST_Relation &a)
{
    s << a.f << endl;

    for (EST_Item *p = a.head(); p; p = p->next())
	s << *p << endl;

    return s;
}


