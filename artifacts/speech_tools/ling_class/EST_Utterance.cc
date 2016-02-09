/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                      Copyright (c) 1995,1996                          */
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
/*                EST_Utterance class source file                        */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include "EST_error.h"
#include "EST_string_aux.h"
#include "ling_class/EST_Utterance.h"
#include "EST_UtteranceFile.h"
#include "EST_string_aux.h"

const EST_String DEF_FILE_TYPE = "est_ascii";

static void clear_up_sisilist(EST_TKVL<EST_Item_Content *,EST_Item *> &s);
static EST_Item *map_ling_item(EST_Item *si,
				    EST_TKVL<EST_Item_Content *,
				        EST_Item *> &s);
static void copy_relation(EST_Item *to,EST_Item *from,
			  EST_TKVL<EST_Item_Content *,EST_Item *> &slist);

Declare_KVL_T(EST_Item_Content *, EST_Item *, KVL_ICP_IP)

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TList.cc"
#include "../base_class/EST_TKVL.cc"

Instantiate_KVL_T(EST_Item_Content *, EST_Item *, KVL_ICP_IP)
#endif

EST_Utterance::EST_Utterance()
{
    init();
}

void EST_Utterance::init()
{
    highest_id = 0;
    f.set("max_id", 0);
}

int EST_Utterance::next_id()
{
    int i = f.val("max_id").Int();
    f.set("max_id", i+1);
    return i+1;
}

void EST_Utterance::clear()
{
    relations.clear();
}

void EST_Utterance::clear_relations()
{
    EST_Features::Entries p;
    for (p.begin(relations); p; p++)
	::relation(p->v)->clear();
}

EST_Relation *EST_Utterance::create_relation(const EST_String &n)
{
    EST_Relation *r = relation(n,FALSE);
    if (r)   // there is one already, so clear it
	r->clear();
    else
    {
	r = new EST_Relation(n);
	r->set_utt(this);
	relations.set_val(n,est_val(r));
    }

    return r;
}

static EST_Item *item_id(EST_Item *p, const EST_String &n)
{
    EST_Item *s, *t;

    t = 0;
    if ((p == 0) || (p->S("id","0") == n))
	return p;

    for (s = daughter1(p); s; s = s->next())
    {
	t = item_id(s, n);
	if (t != 0)
	    return t;
    }

    return 0;
}

EST_Item *EST_Utterance::id(const EST_String &n) const
{
    EST_Item *s, *t;
    EST_Features::Entries p;
    
    for (p.begin(relations); p; p++)
	for (s = ::relation(p->v)->head(); s; s = next_item(s))
	    if ((t = item_id(s, n)) != 0)
		return t;
    EST_error("Could not find item matching id %s\n", (const char *)n);
    return 0;
}

void EST_Utterance::evaluate_all_features()
{
    EST_Features::Entries p;

    for (p.begin(relations); p; p++)
	::relation(p->v)->evaluate_item_features();
}

void EST_Utterance::remove_relation(const EST_String &n)
{
    EST_Relation *r = relation(n,FALSE);

    if (r != 0)
	relations.remove(n);
}

EST_Relation *EST_Utterance::relation(const char *name,int err) const
{
    if (err)
	return ::relation(relations.f(name));
    else
    {
	EST_Relation *r = 0;
	return ::relation(relations.f(name,est_val(r)));
    }
}

bool EST_Utterance::relation_present(const EST_String name) const
{
    if (!name.contains("("))
	return relations.present(name);
    EST_StrList s;
    BracketStringtoStrList(name, s);
    return relation_present(s);
}

bool EST_Utterance::relation_present(EST_StrList &names) const
{
    for (EST_Litem *p = names.head(); p ; p = p->next())
	if (!relations.present(names(p)))
	    return false;
    return true;
}

EST_Utterance &EST_Utterance::operator=(const EST_Utterance &s)
{
    copy(s);
    return *this;
}

ostream& operator << (ostream &st, const EST_Utterance &u)
{
    u.save(st,"est_ascii");
    return st;
}

void EST_Utterance::copy(const EST_Utterance &u)
{
    // Make a copy of the utterance
    EST_TKVL<EST_Item_Content *,EST_Item *> sisilist;
    EST_Relation *nrel;
    EST_Item *rnode;

    clear();
    f = u.f;

    EST_Features::Entries r;
    for (r.begin(u.relations); r; r++)
    {
	EST_Relation *rr = ::relation(r->v);
	nrel = create_relation(rr->name());
	nrel->f = rr->f;
	if (rr->head() != 0)
	{
	    rnode = nrel->append(map_ling_item(rr->head(),sisilist));
	    copy_relation(rnode,rr->head(),sisilist);
	}
    }
    clear_up_sisilist(sisilist);
}

static void extra_sub_utterance(EST_Utterance &u,EST_Item *i)
{
    sub_utterance(u,i);
}

void EST_Utterance::sub_utterance(EST_Item *i)
{
    extra_sub_utterance(*this,i);
}

static void merge_tree(EST_Relation *urel, 
		       EST_Relation *rel,
		       EST_Item *uroot,
		       EST_Item *root,
		       EST_Features &items,
		       EST_String feature)
{
    EST_Item *n=0;
    merge_features(uroot->features(), root->features());
    // copy horizontally
    if (root->next()!= NULL)
    {
	EST_Item *old = item(items.f(root->next()->S(feature),est_val(n)));
	EST_Item *new_root = old?uroot->insert_after(old):uroot->insert_after();
	merge_tree(urel, rel, new_root, root->next(), items, feature);
    }
    // vertically
    if (root->down()!= NULL)
    {
	EST_Item *old = item(items.f(root->down()->S(feature),est_val(n)));
	EST_Item *new_root = old?uroot->insert_below(old):uroot->insert_below();
	merge_tree(urel, rel, new_root, root->down(), items, feature);
    }
}

int utterance_merge(EST_Utterance &utt,
		    EST_Utterance &extra,
		    EST_String feature)
{
    // Global merge. Uses the feature to determine which items correspond.

    // First build a table of existing contents.

    EST_Features items;
    EST_Features::Entries ri;
    for(ri.begin(utt.relations); ri; ri++)
    {
	EST_Relation *rel = relation(ri->v);
	for(EST_Item *i=rel->head(); i != NULL; i=i->next_item())
	{
	    EST_String id = i->S(feature);
	    items.set_val(id,est_val(i));
	}
    }

    EST_Features::Entries eri;
    for(eri.begin(extra.relations); eri; eri++)
    {
	EST_Relation *rel = relation(eri->v);
	EST_String rel_name = rel->name();

	while (utt.relation_present(rel_name))
	    rel_name += "+";

	EST_Relation *urel = utt.create_relation(rel_name);

	if (rel->head() != NULL)
	{
	    EST_Item *n = 0;
	    EST_Item *old = item(items.f(rel->head()->S(feature),est_val(n)));
	    EST_Item *new_root = old?urel->append(old):urel->append();
	    merge_tree(urel, rel, new_root, rel->head(), items, feature);
	}
    }

    return TRUE;
}

int utterance_merge(EST_Utterance &utt,
		    EST_Utterance &sub_utt,
		    EST_Item *utt_root,
		    EST_Item *sub_root)
{
    // Joins sub_utt to utt at ling_item at, merging the root
    // of relname in sub_utt with ling_item at.  All other relations
    // in sub_utt get their root's appended (not merged) with the
    // corresponding relations in utt (and created if necessary).
    EST_TKVL<EST_Item_Content *,EST_Item *> sisilist;
    EST_Item *rnode;
    EST_Relation *nrel;

    if (utt_root->relation_name() != sub_root->relation_name())
	EST_error("utterance_merge: items not is same relation");

    if ((utt_root == 0) || (sub_root == 0))
	EST_error("utterance_merge: items are null");

    // merge features but preserve root id
    EST_String root_id = utt_root->S("id");
    merge_features(utt_root->features(), sub_root->features());
    utt_root->set("id", root_id);
    // in case root item in sub is referenced elsewhere in the structure
    sisilist.add_item(sub_root->contents(),utt_root);
    copy_relation(utt_root,sub_root,sisilist);
    
    EST_Features::Entries r;
    for (r.begin(sub_utt.relations); r; r++)
    {
	EST_Relation *rr = ::relation(r->v);
	if (rr->name() != utt_root->relation_name())
	{
	    if (!utt.relation_present(rr->name()))
		nrel = utt.create_relation(rr->name());
	    else
		nrel = utt.relation(rr->name());
	    if (rr->head() != 0)
	    {
		EST_Item *nn = map_ling_item(rr->head(),sisilist);
		rnode = nrel->append(nn);
		copy_relation(rnode,rr->head(),sisilist);
	    }
	}
    }
    sisilist.remove_item(sub_root->contents());
    clear_up_sisilist(sisilist);
    return TRUE;
}

static void copy_relation(EST_Item *to,EST_Item *from,
 		          EST_TKVL<EST_Item_Content *,EST_Item *> &slist)
{
    // Construct next and down nodes of from, into to, mapping
    // stream_items through slist

    if (from->next())
	copy_relation(to->insert_after(map_ling_item(from->next(),slist)),
		      from->next(),
		      slist);
    if (from->down())
	copy_relation(to->insert_below(map_ling_item(from->down(),slist)),
		      from->down(),
		      slist);
}

static EST_Item *map_ling_item(EST_Item *si,
			       EST_TKVL<EST_Item_Content *,EST_Item *> &s)
{
    // If si is already in s return its map otherwise copy
    // si and add it to the list
    EST_Item *msi;
    EST_Item *def = 0;
    
    msi = s.val_def(si->contents(),def);
    if (msi == def)
    {   // First time, so copy it and add to map list
	msi = new EST_Item(*si);
	msi->f_remove("id");
	s.add_item(si->contents(),msi);
    }
    return msi;
}

static void clear_up_sisilist(EST_TKVL<EST_Item_Content *,EST_Item *> &s)
{
    // The EST_Items in the value of this need to be freed, its
    // contents however will not be freed as they will be referenced
    // somewhere in the copied utterance
    
    for (EST_Litem *r=s.list.head(); r != 0; r=r->next())
	delete s.list(r).v;

}

static EST_Item *mapped_parent(EST_Item *i,const EST_String &relname,
			       EST_TKVL<EST_Item_Content *,EST_Item *> &s)
{
    EST_Item *p;

    if ((p=parent(i,relname)) == 0)
	return 0;
    else if (s.present(p->contents()))
	return map_ling_item(p,s)->as_relation(relname);
    else
	return 0;
}

static void sub_utt_copy(EST_Utterance &sub,EST_Item *i,
			 EST_TKVL<EST_Item_Content *,EST_Item *> &s)
{
    if (s.present(i->contents()))
	return;
    else
    {
	EST_Item *np,*d;
	EST_Litem *r;
	EST_Item *ni = map_ling_item(i,s);
	for (r = i->relations().list.head(); r; r = r->next())
	{
	    EST_String relname = i->relations().list(r).k;
	    if (!sub.relation_present(relname))
		sub.create_relation(relname)->append(ni);
	    else if ((np=mapped_parent(i,relname,s)) != 0)
		np->append_daughter(ni);
	    else
		sub.relation(relname)->append(ni);

	    // Do its daughters
	    for (d = daughter1(i,relname); d ; d=d->next())
		sub_utt_copy(sub,d,s);
	}
    }
}

void sub_utterance(EST_Utterance &sub,EST_Item *i)
{
    // Extract i and all its relations, and daughters ... to build
    // a new utterance in sub.
    EST_TKVL<EST_Item_Content *,EST_Item *> sisilist;

    sub.clear();
    sub_utt_copy(sub,i,sisilist);
    
    clear_up_sisilist(sisilist);
}

EST_read_status EST_Utterance::load(const EST_String &filename)
{   
    EST_TokenStream ts;
    EST_read_status v=format_ok;
    
    if (((filename == "-") ? ts.open(cin) : ts.open(filename)) != 0)
    {
	cerr << "load_utt: can't open utterance input file " 
	    << filename << endl;
	return misc_read_error;
    }

    v = load(ts);

    if (v == read_ok)
      f.set("filename", filename);

    ts.close();

    return v;
}

EST_read_status EST_Utterance::load(EST_TokenStream &ts)
{
    EST_read_status stat=read_error;
    int pos = ts.tell();
    int max_id;

    init();  // we're committed to reading something so clear utterance

    for(int n=0; n< EST_UtteranceFile::map.n() ; n++)
    {
	EST_UtteranceFileType t = EST_UtteranceFile::map.token(n);
	
	if (t == uff_none)
	    continue;
	
	EST_UtteranceFile::Info *info = &(EST_UtteranceFile::map.info(t));
	
	if (! info->recognise)
	    continue;
	
	EST_UtteranceFile::Load_TokenStream * l_fun = info->load;
	
	if (l_fun == NULL)
	    continue;

	ts.seek(pos);

	stat = (*l_fun)(ts, *this, max_id);
	
	if (stat == read_ok)
	{
	  // set_file_type(EST_UtteranceFile::map.value(t));
	    break;
	}
    }
    
    highest_id = max_id;
    return stat;
}

EST_write_status EST_Utterance::save(const EST_String &filename,
				     const EST_String &type) const
{
    EST_write_status v;
    ostream *outf;
    
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))
	return write_fail;

    v = save(*outf,type);

    if (outf != &cout)
	delete outf;

    return v;
}

EST_write_status EST_Utterance::save(ostream &outf,
				     const EST_String &type) const
{
      EST_String save_type = (type == "") ? DEF_FILE_TYPE : type;

    EST_UtteranceFileType t = EST_UtteranceFile::map.token(save_type);

    if (t == uff_none)
    {
	cerr << "Utterance: unknown filetype in saving " << save_type << endl;
	return write_fail;
    }
    
    EST_UtteranceFile::Save_TokenStream * s_fun = EST_UtteranceFile::map.info(t).save;
    
    if (s_fun == NULL)
    {
	cerr << "Can't save utterances to files type " << save_type << endl;
	return write_fail;
    }
    
    return (*s_fun)(outf, *this);
}

void utt_2_flat_repr( const EST_Utterance &utt,
		      EST_String &flat_repr )
{
  EST_Item *phrase = utt.relation("Phrase")->head();
  for( ; phrase; phrase=phrase->next() ){
    flat_repr += "<";
 
    EST_Item *word = daughter1(phrase);
    for( ; word; word=word->next() ){
      flat_repr += "{";

      EST_Item *syllable = daughter1(word, "SylStructure");
      for( ; syllable; syllable=syllable->next() ){
	flat_repr += EST_String::cat( "(", syllable->S("stress") );

	EST_Item *phone = daughter1(syllable);
	for( ; phone; phone=phone->next() )
	  flat_repr += EST_String::cat( " ", phone->S("name"), " " );
	flat_repr += ")";
      }
      flat_repr += "}";
    }
    flat_repr += EST_String::cat( "> _", phrase->S("name"), " " ); 
  }
}
