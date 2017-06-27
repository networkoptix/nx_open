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
/*                       Author :  Paul Taylor and Simon King            */
/*                       Date   :  June 1995                             */
/*-----------------------------------------------------------------------*/
/*                  Relation class auxiliary routines                    */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cmath>
#include "EST_types.h"
#include "ling_class/EST_Relation.h"
#include "ling_class/EST_relation_aux.h"
#include "EST_string_aux.h"
#include "EST_io_aux.h"
#include "EST_Option.h"
#include "EST_Token.h"

static int is_in_class(const EST_String &name, EST_StrList &s);

bool dp_match(const EST_Relation &lexical,
	      const EST_Relation &surface,
	      EST_Relation &match,
	      float ins, float del, float sub);


float start(EST_Item *n)
{
    return (n->prev() == 0) ? 0.0 : n->prev()->F("end");
}

float duration(EST_Item *n)
{
    return n->F("end") - start(n);
}

void quantize(EST_Relation &a, float q)
{
    EST_Item *a_ptr;
    float end;

    for (a_ptr = a.head(); a_ptr != 0; a_ptr = a_ptr->next())
    {
	end = a_ptr->F("end") / q;
	end = rint(end);
	end = end * q;
	a_ptr->set("end", end);
    }
}

// edit labels using a sed file to do the editing

int edit_labels(EST_Relation &a, EST_String sedfile)
{
    EST_Item *a_ptr;
    char command[100], name[100], newname[100], sf[100];
    FILE *fp;
    strcpy(sf, sedfile);
    EST_String file1, file2;
    file1 = make_tmp_filename();
    file2 = make_tmp_filename();

    fp = fopen(file1, "wb");
    if (fp == NULL)
    {
	fprintf(stderr,"edit_labels: cannot open \"%s\" for writing\n",
		(const char *)file1);
	return -1;
    }
    for (a_ptr = a.head(); a_ptr != 0; a_ptr = a_ptr->next())
    {
	strcpy(name,  a_ptr->name());
	fprintf(fp, "%s\n", name);
    }
    fclose(fp);
    strcpy(command, "cat ");
    strcat(command, file1);
    strcat(command, " | sed -f ");
    strcat(command, sedfile);
    strcat(command, " > ");
    strcat(command, file2);

    printf("command: %s\n", command);
    system(command);

    fp = fopen(file2, "rb");
    if (fp == NULL)
    {
	fprintf(stderr,"edit_labels: cannot open \"%s\" for reading\n",
		(const char *)file2);
	return -1;
    }
    for (a_ptr = a.head(); a_ptr != 0; a_ptr = a_ptr->next())
    {
	fscanf(fp, "%s", newname);
//	cout << "oldname: " << a_ptr->name() << " newname: " << newname << endl;
	a_ptr->set_name(newname);
    }
    fclose(fp);
    return 0;
}

// make new EST_Relation from start and end points.
void extract(const EST_Relation &orig, float s,
	     float e, EST_Relation &ex)
{
    EST_Item *a;
    EST_Item *tmp;

    for (a = orig.head(); a != 0; a = a->next())
	if ((a->F("end") > s) && (start(a) < e))
	{
	    tmp = ex.append(a);
	    if ((a->F("end") > e))
		tmp->set("end", e);
	}
}

void merge_all_label(EST_Relation &seg, const EST_String &labtype)
{
    EST_Item *a_ptr, *n_ptr;
    (void)labtype;  // unused parameter

    for (a_ptr = seg.head(); a_ptr != seg.tail(); a_ptr = n_ptr)
    {
	n_ptr = a_ptr->next();
	if (a_ptr->name() == a_ptr->next()->name())
	    seg.remove_item(a_ptr);
    }
}

void change_label(EST_Relation &seg, const EST_String &oname, 
		  const EST_String &nname)
{
    EST_Item *a_ptr;

    for (a_ptr = seg.head(); a_ptr != 0; a_ptr = a_ptr->next())
	if (a_ptr->name() == oname)
	    a_ptr->set_name(nname);
}

void change_label(EST_Relation &seg, const EST_StrList &oname, 
		  const EST_String &nname)
{
    EST_Item *a_ptr;
    EST_Litem *p;

    for (a_ptr = seg.head(); a_ptr != 0; a_ptr = a_ptr->next())
	for (p = oname.head(); p ; p = p->next())
	    if (a_ptr->name() == oname(p))
		a_ptr->set_name(nname);
}

static int is_in_class(const EST_String &name, EST_StrList &s)
{
    EST_Litem *p;

    for (p = s.head(); p; p = p->next())
	if (name == s(p))
	    return TRUE;
    
    return FALSE;
}

int check_vocab(EST_Relation &a, EST_StrList &vocab)
{
    EST_Item *s;
    for (s = a.head(); s; s = s->next())
	if (!is_in_class(s->name(), vocab))
	{
	    cerr<<"Illegal entry in file " <<a.name()<< ":\""  << *s << "\"\n";
	    return -1;
	}
    return 0;
}

void convert_to_broad_class(EST_Relation &seg, const EST_String &class_type, 
			   EST_Option &options)
{
    // class_type contains a list of whitepsace separated segment names.
    // This function looks at each segment and adds a feature "pos"
    // if its name is contained in the list.
    EST_String tmp_class_type = class_type + "_list";
    EST_String bc_list(options.val(tmp_class_type, 1));
    EST_StrList pos_list;
    EST_TokenStream ts;
    
    ts.open_string(bc_list);
    while (!ts.eof())
        pos_list.append(ts.get().string());

    convert_to_broad(seg, pos_list);
} 

void convert_to_broad(EST_Relation &seg, EST_StrList &pos_list, 
		      EST_String broad_name, int polarity)
{
    EST_Item *a_ptr;
    if (broad_name == "")
	broad_name = "pos";

    for (a_ptr = seg.head(); a_ptr != 0; a_ptr = a_ptr->next())
	if (is_in_class(a_ptr->name(), pos_list))
	    a_ptr->set(broad_name, (polarity) ? 1 : 0);
	else
	    a_ptr->set(broad_name, (polarity) ? 0 : 1);
} 

void label_map(EST_Relation &seg, EST_Option &map)
{
    EST_Item *p, *n;
    
    for (p = seg.head(); p != 0; n = p)
    {
	n = p->next();
	if (map.present(p->name()))
	{
	    if (map.val(p->name()) == "!DELETE")
		seg.remove_item(p);
	    else
		p->set_name(map.val(p->name()));
	}

    }
} 

void shift_label(EST_Relation &seg, float shift)
{
    //shift every end time by adding x seconds.
    EST_Item *a_ptr;
    
    for (a_ptr = seg.head(); a_ptr != 0; a_ptr = a_ptr->next())
	a_ptr->set("end", a_ptr->F("end") + shift);
}

void RelationList_select(EST_RelationList &mlf, EST_StrList filenames, bool
			exact_match)
{
    // select only files in 'filenames'
    // remove all others from mlf
    EST_Litem *fptr, *ptr;
    bool flag;
    
    // if not exact match, only match basenames
    EST_StrList tmp_filenames;
    for (ptr = filenames.head(); ptr != NULL; ptr = ptr->next())
	if(exact_match)
	    tmp_filenames.append( filenames(ptr) );
	else
	    tmp_filenames.append( basename(filenames(ptr)) );
    
    for(fptr=mlf.head(); fptr != NULL;)
    {
	flag=false;
	for (ptr = tmp_filenames.head(); ptr != NULL; ptr = ptr->next())
	    if(exact_match)
	    {
		if(tmp_filenames(ptr) == mlf(fptr).name())
		{
		    flag=true;
		    break;
		}
	    }
	    else if(mlf(fptr).name().contains(tmp_filenames(ptr)))
	    {
		flag=true;
		break;
	    }
	
	if(!flag)
	{
	    fptr = mlf.remove(fptr);
	    
	    if(fptr==0)			// must have removed head of list
		fptr=mlf.head();
	    else
		fptr=fptr->next();
	}
	else
	    fptr=fptr->next();
    }
    tmp_filenames.clear();
}

// look for a single file called "filename" and make a EST_Relation out of
// this
EST_Relation RelationList_extract(EST_RelationList &mlf, const EST_String &filename, bool base)
{

    EST_Litem *p;
    EST_String test, ref;

    if (base)
	for (p = mlf.head(); p; p = p->next())
	{
	    if (basename(mlf(p).name(), "*")==basename(filename, "*"))
		return mlf(p);
	}
    else 
	for (p = mlf.head(); p; p = p->next())
	{	
	    if (basename(mlf(p).name()) == filename)
		return mlf(p);
	}
    
    cerr << "No match for file " << filename << " found in mlf\n";
    EST_Relation d;
    return d;
}

// combine all relation in MLF into a single relation. 
EST_Relation RelationList_combine(EST_RelationList &mlf)
{
    EST_Litem *p;
    EST_Relation all;
    EST_Item *s, *t = 0;
    float last = 0.0;

    for (p = mlf.head(); p; p = p->next())
    {
	for (s = mlf(p).head(); s; s = s->next())
	{
	    t = all.append();
	    t->set("name", s->S("name"));
	    t->set("end", s->F("end") + last);
	    cout << "appended t " << t << endl;
	}
	last = (t != 0) ? t->F("end") : 0.0;
    }
    return all;
}

EST_Relation RelationList_combine(EST_RelationList &mlf, EST_Relation &key)
{
    EST_Litem *p;
    EST_Relation all;
    EST_Item *s, *t = 0, *k;
    float st;

    if (key.length() != mlf.length())
    {
	cerr << "RelationList has " << mlf.length() << " elements: expected "
	    << key.length() << " from key file\n";
	return all;
    }
    
    for (k = key.head(), p = mlf.head(); p; p = p->next(), k = k->next())
    {
	st = start(k);
	for (s = mlf(p).head(); s; s = s->next())
	{
	    t = all.append();
	    t->set("name", s->S("name"));
	    t->set("end", (s->F("end") + st));
	}
    }
    return all;
}

int relation_divide(EST_RelationList &slist, EST_Relation &lab, 
		    EST_Relation &keylab,
		    EST_StrList &blank, EST_String ext)
{ // divides a single relation into multiple chunks according to the
    // keylab relation. If the keylab boundary falls in the middle of a label,
    // the label is assigned to the chunk which has the most overlap with
    // it. Some labels may be specified in the "blank" list which means thy
    // are duplicated across boundaries.
    
    EST_Relation a, newkey;
    EST_Item *s, *k, *t = 0, *n;
    EST_String filename;
    float kstart;
    
    slist.clear();
    
    if ((keylab.tail())->F("end") < (lab.tail())->F("end"))
    {
	cerr << "Key file must extend beyond end of label file\n";
	return -1;
    }

    // find a the first keylab that will make a non-empty file
    for (k = keylab.head(); k ; k = k->next())
	if (k->F("end") > lab.head()->F("end"))
	    break;

    filename = (EST_String)k->f("file");
    a.f.set("name", (filename + ext));
    kstart = 0.0;
    
    for (s = lab.head(); s; s = s->next())
    {
	n = s->next();
	if (n == 0)
	{
	    t = a.append(s);
	    t->set("end", (s->F("end") - kstart));
	    break;
	}
	if (n->F("end") > k->F("end"))
	{
	    if (((n->F("end") - k->F("end")) < 
		 (k->F("end") - start(n))) || 
		is_in_class(n->name(), blank))
	    {
		a.append(s);
		t->set("end", (s->F("end") - kstart));

		t = a.append(n);
		t->set("end", (k->F("end") - kstart));

		if (!is_in_class(n->name(), blank))
		    s = s->next();
	    }
	    else
	    {
		t = a.append(s);
		t->set("end", (k->F("end") - kstart));
	    }
	    
	    slist.append(a);
	    k = k->next();
	    kstart = start(k);
	    a.clear();
	    filename = (EST_String)k->f("file");
	    a.f.set("name", (filename + ext));
	}
	else
	{
	    t = a.append(s);
	    t->set("end", (s->F("end") - kstart));
	}
    }
    slist.append(a);
    
    return 0;
}

int relation_divide2(EST_RelationList &mlf, EST_Relation &lab, 
		     EST_Relation &keylab, EST_String ext)
{
    EST_Relation a, newkey;
    EST_Item *s, *k, *t;
    float kstart;
    
    mlf.clear();
    
    if ((keylab.tail())->F("end") < (lab.tail())->F("end"))
    {
	cerr << "Key file must extend beyond end of label file\n";
	return -1;
    }
    
    k = keylab.head();
    a.f.set("name", (k->name() + ext));
    kstart = 0.0;
    
    for (s = lab.head(); s; s = s->next())
    {
	t = a.append();
	t->set_name(s->name());
	t->set("end", (s->F("end") - kstart));
	
	if (s->F("end") > k->F("end"))
	{
	    cout << "appending " << a;
	    mlf.append(a);
	    
	    kstart = s->F("end");
	    k->set("end", (s->F("end")));
	    k = k->next();
	    a.clear();
	    a.f.set("name", (k->name() + ext));
	}
    }
    cout << "appending " << a;
    mlf.append(a);
    
    return 0;
}




void map_match_times(EST_Relation &target, const EST_String &match_name,
	       const EST_String &time_name, bool do_start)
{
    EST_Item *s, *t, *p;
    float prev_end, inc, first_end, last_end;
    int i;

    // first pass, copy times as appropriate, and find first 
    // and last defined ends
    // This is hacky and certainly won't work for many cases

    first_end = -1.0;
    prev_end = 0.0;
    last_end = 0.0;

//    cout << "surface: " << surface << endl;

    for (s = target.head(); s; s = s->next())
    {
	if ((t = daughter1(s->as_relation(match_name))) != 0)
	{
	    s->set(time_name + "end", t->F("end"));
	    if (do_start)
		s->set(time_name + "start", t->F("start"));
	    
	    last_end = t->F("end");
	    if (first_end < 0.0)
		first_end = t->F("end");
	}
    }

     if (!target.head()->f_present(time_name + "end"))
     {
	 target.head()->set(time_name + "end", first_end / 2.0);
	 if (do_start)
	     target.head()->set(time_name + "start", 0.0);
     }

     if (!target.tail()->f_present(time_name + "end"))
     {
	 target.tail()->set(time_name + "end", last_end + 0.01);
	 if (do_start)
	     target.tail()->set(time_name + "start", last_end);
     }

    for (s = target.head(); s; s = s->next())
    {
	if (!s->f_present(time_name + "end"))
	{
//	    cout << "missing end feature for " << *s << endl;
	    for (i = 1, p = s; p; p = p->next(), ++i)
		if (p->f_present(time_name + "end"))
		    break;
	    inc = (p->F(time_name + "end") - prev_end) / ((float) i);
//	    cout << "inc is : " << inc << endl;

//	    cout << "stop phone is " << *p << endl;

	    for (i = 1; s !=p ; s = s->next(), ++i)
	    {
		s->set(time_name + "end", (prev_end + ((float) i * inc)));
		if (do_start)
		s->set(time_name + "start", (prev_end+((float) (i - 1 )* inc)));
	    }
	}
	prev_end = s->F("end");
    }
}	    

void dp_time_align(EST_Utterance &utt, const EST_String &source_name,
		   const EST_String &target_name, 
		   const EST_String &time_name,
		   bool do_start)
{
    utt.create_relation("Match");

    dp_match(*utt.relation(target_name), *utt.relation(source_name), 
	     *utt.relation("Match"), 7.0, 7.0, 7.0);

    map_match_times(*utt.relation(target_name), "Match", time_name, do_start);
}


EST_Litem *RelationList_ptr_extract(EST_RelationList &mlf, const EST_String &filename, bool base)
{
    EST_Litem *p;
    EST_String test, ref;
    
    if (base)
	for (p = mlf.head(); p; p = p->next())
	{
	    if (basename(mlf(p).name(), "*")==basename(filename, "*"))
		return p;
	}
    else 
	for (p = mlf.head(); p; p = p->next())
	    if (mlf(p).name() == filename)
		return p;
    
    cerr << "No match for file " << filename << " found in mlf\n";
    return 0;
}

void relation_convert(EST_Relation &lab, EST_Option &al, EST_Option &op)
{
    if (al.present("-shift"))
	shift_label(lab, al.fval("-shift"));
    
    // fix option later.    
    if (al.present("-extend"))
	al.override_fval("-length", 
			 al.fval("-extend",0) * lab.tail()->F("end"));
    
    // quantize (ie round up or down) label times
    if (al.present("-q"))
	quantize(lab, al.fval("-q"));
    
    if (al.present("-start"))
    {
	if (!al.present("-end"))
	    cerr << "-start option must be used with -end option\n";
	else 
	    extract(lab, al.fval("-start"), al.fval("-end"), lab);
    }
    
    if (al.present("-class"))
	convert_to_broad_class(lab, al.val("-class"), op);
    
    else if (al.present("-pos"))
    {
	EST_StrList bclass;
	StringtoStrList(al.val("-lablist"), bclass);
	convert_to_broad(lab, bclass);
    }
    else if (al.present("-sed"))
	edit_labels(lab, al.val("-sed"));
    else if (al.present("-map"))
    {
	EST_Option map;
	if (map.load(al.val("-map")) != format_ok)
	    return;
	label_map(lab, map);
    }
}

    

void print_relation_features(EST_Relation &stream)
{
    EST_Item *s;
    EST_Features::Entries p;

    for (s = stream.head(); s; s = s->next())
    {
	cout << s->name() << "\t:";
	for(p.begin(s->features()); p; ++p)
	    cout << p->k << " " 
		<< p->v << "; ";
	cout << endl;
    }
    
}


void build_RelationList_hash_table(EST_RelationList &mlf,
				   EST_hashedRelationList &hash_table, 
				   const bool base)
{
  EST_Litem *p;
  if (base)
    for (p = mlf.head(); p; p = p->next())
	hash_table.add_item(basename(mlf(p).name(), "*"),
			    &(mlf(p)));
  else 
    for (p = mlf.head(); p; p = p->next())
      hash_table.add_item(mlf(p).name(),
			      &(mlf(p)));
}


bool hashed_RelationList_extract(EST_Relation* &rel,
				 const EST_hashedRelationList &hash_table,
				 const EST_String &filename, bool base)
{
  EST_Relation *d;
  EST_String fname = filename;
  int found;
  
  if (base)
    fname=basename(filename, "*");
  
  d=hash_table.val(fname,found);
  
  if(found)
    {
      rel = d;
      return true;
    }
  cerr << "No match for file " << fname << " found in mlf\n";
  return false;
}


