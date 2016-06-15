/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                      Copyright (c) 1996-1998                          */
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
/*                     Author :  Alan W Black                            */
/*                     Date   :  February 1998                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Functions to add Speech Tools basic objects to the SIOD LISP obj      */
/*                                                                       */
/* This offers non-intrusive support for arbitrary objects in LISP,      */
/* however because the deletion method are called this needs to access   */
/* Thus if you include siod_est_init(), you'll get Utterances, Nodes     */
/* Stream_Items, Waves and Tracks in your binary                         */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include "siod.h"
#include "ling_class/EST_Utterance.h"
#include "ling_class/EST_Item.h"
#include "EST_THash.h"
#include "EST_Wave.h"
#include "EST_wave_aux.h"
#include "EST_Track.h"
#include "EST_track_aux.h"

Declare_TStringHash_Base(LISP,(LISP)0,NIL)

#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_THash.cc"

Instantiate_TStringHash(LISP)

#endif

// To make garbage collection easy the following functions offer an index
// of arbitrary objects to LISP cells.  You can use this to return the
// same LISP cell for the same object.  This is used for utterance
// objects otherwise I'd need to add reference counts to the utterance
// itself
//
// This is implemented as a hash table of printed address
// This if fine for hundreds of things, but probably not
// for thousands of things
static EST_TStringHash<LISP> estobjs(100);

static void void_to_addrname(const void *v,EST_String &saddr)
{
    char addr[128];

    sprintf(addr,"%p",v);
    saddr = addr;
}

// The following are the types for EST objects in LISP, they are set when
// the objects are registered.  I don't think they should be required 
// out side this file so they are static functions like siod_utterance_p
// should be used elsewhere
static int tc_utt = -1;
static int tc_val = -1;

class EST_Utterance *utterance(LISP x)
{
    if (TYPEP(x,tc_utt))
	return (class EST_Utterance *)USERVAL(x);
    else
	err("wrong type of argument to get_c_utt",x);

    return NULL;  // err doesn't return but compilers don't know that
}

int utterance_p(LISP x)
{
    if (TYPEP(x,tc_utt))
	return TRUE;
    else
	return FALSE;
}

LISP siod(const class EST_Utterance *u)
{
    LISP utt;
    EST_String saddr;
    LISP cell;

    void_to_addrname(u,saddr);

    if ((cell = estobjs.val(saddr)) != NIL)
	return cell;

    // A new one 
    utt = siod_make_typed_cell(tc_utt,(void *)u);

    // Add to list
    estobjs.add_item(saddr,utt);

    return utt;
}

static void utt_free(LISP lutt)
{
    class EST_Utterance *u = utterance(lutt);
    EST_String saddr;

    void_to_addrname(u,saddr);

    // Mark it unused, this doesn't gc the extra data in the hash
    // table to hold the index, this might be a problem over very
    // long runs of the system (i.e. this should be fixed).
    estobjs.remove_item(saddr);
    delete u;
    

    USERVAL(lutt) = NULL;
}

LISP utt_mark(LISP utt)
{
    // Should mark all the LISP cells in it 
    // but at present we use the gc_(un)protect mechanism 
    return utt;
}

// EST_Vals (and everything else)
class EST_Val &val(LISP x)
{
    if (TYPEP(x,tc_val))
	return *((class EST_Val *)x->storage_as.val.v);

    else
	err("wrong type of argument to get_c_val",x);
    // sigh
    static EST_Val def;

    return def;
}

LISP val_equal(LISP a,LISP b)
{
    if (val(a) == val(b))
	return truth;
    else
	return NIL;
}

int val_p(LISP x)
{
    if (TYPEP(x,tc_val))
	return TRUE;
    else
	return FALSE;
}

LISP siod(const class EST_Val v)
{
    return siod_make_typed_cell(tc_val,new EST_Val(v));
}

static void val_free(LISP val)
{
    class EST_Val *v = (EST_Val *)USERVAL(val);
    delete v;
    USERVAL(val) = NULL;
}

static void val_prin1(LISP v, FILE *fd)
{
    char b[1024];
    fput_st(fd,"#<");
    fput_st(fd,val(v).type());
    sprintf(b," %p",val(v).internal_ptr());
    fput_st(fd,b);
    fput_st(fd,">");
}

static void val_print_string(LISP v, char *tkbuffer)
{
    sprintf(tkbuffer,"#<%s %p>",val(v).type(),val(v).internal_ptr());
}

SIOD_REGISTER_CLASS(item,EST_Item)
SIOD_REGISTER_CLASS(wave,EST_Wave)
SIOD_REGISTER_CLASS(track,EST_Track)
SIOD_REGISTER_CLASS(feats,EST_Features)

// This is an example of something that's a little scary and it
// would be better if we didn't have to do this.  Here we define
// support for LISP's as VAL, even though we've got VAL's a LISPs
// This allows arbitrary LISP objects to be held as VALs most
// likely as values in features or being returned by feature functions
// We have to do some special memory management to do this and 
// you can probably mess things up completely if you start using this
// arbitrarily
val_type val_type_scheme = "scheme";
struct obj_val {LISP l;};
LISP scheme(const EST_Val &v)
{
    if (v.type() == val_type_scheme)
	return ((obj_val *)v.internal_ptr())->l;
    else
	EST_error("val not of type val_type_scheme");
    return NULL;
}
static void val_delete_scheme(void *v)
{
    struct obj_val *ov = (struct obj_val *)v;
    gc_unprotect(&ov->l);
    wfree(ov);
}

EST_Val est_val(const obj *v)
{
    struct obj_val *ov = walloc(struct obj_val,1);
    ov->l = (LISP)(void *)v;
    gc_protect(&ov->l);
    return EST_Val(val_type_scheme,
		   (void *)ov,
		   val_delete_scheme);
}

LISP lisp_val(const EST_Val &pv)
{
    if (pv.type() == val_unset)
    {
	cerr << "EST_Val unset, can't build lisp value" << endl;
	siod_error();
	return NIL;
    }
    else if (pv.type() == val_int)
	return flocons(pv.Int());
    else if (pv.type() == val_float)
	return flocons(pv.Float());
    else if (pv.type() == val_string)
	return strintern(pv.string_only());
    else if (pv.type() == val_type_scheme)
	return scheme(pv);
    else if (pv.type() == val_type_feats)
	return features_to_lisp(*feats(pv));
    else
	return siod(pv);
}

static int feature_like(LISP v)
{
    // True if non nil and assoc like
    if ((v == NIL) || (!consp(v)))
	return FALSE;
    else
    {
	LISP p;
	for (p=v; p != NIL; p=cdr(p))
	{
	    if (!consp(p) || (!consp(car(p))) || (consp(car(car(p)))))
		return FALSE;
	}
	return TRUE;
    }
}

EST_Val val_lisp(LISP v)
{
    if (feature_like(v))
    {
	EST_Features *f = new EST_Features;
	lisp_to_features(v,*f);
	return est_val(f);
    }
    else if (FLONUMP(v))
	return EST_Val(get_c_float(v));
    else if (TYPEP(v,tc_val))
	return val(v);
    else if (TYPEP(v,tc_symbol) || (TYPEP(v,tc_string)))
	return EST_Val(EST_String(get_c_string(v)));
    else 
	return est_val(v);
}

LISP kvlss_to_lisp(const EST_TKVL<EST_String, EST_String> &kvl)
{
    LISP l = NIL;

    EST_TKVL<EST_String, EST_String>::Entries p;

    for(p.begin(kvl); p; ++p)
      {
	l=cons(cons(rintern(p->k),
		     cons(lisp_val(p->v),NIL)),
		l);
      }
    // reverse it to make it the same order as f, though that shouldn't matter
    return reverse(l);
}

void lisp_to_kvlss(LISP l, EST_TKVL<EST_String, EST_String> &kvl)
{
    LISP p;

    for (p=l; p; p = cdr(p))
	kvl.add_item(get_c_string(car(car(p))),
		     get_c_string(car(cdr(car(p)))));
}

LISP features_to_lisp(EST_Features &f)
{
    LISP lf = NIL;

    EST_Features::Entries p;

    for(p.begin(f); p; ++p)
      {
	lf=cons(cons(rintern(p->k),
		     cons(lisp_val(p->v),NIL)),
		lf);
      }
    // reverse it to make it the same order as f, though that shouldn't matter
    return reverse(lf);
}

void lisp_to_features(LISP lf,EST_Features &f)
{
    LISP p;

    for (p=lf; p; p = cdr(p))
	f.set_val(get_c_string(car(car(p))),
		  val_lisp(car(cdr(car(p)))));
}

static LISP feats_set(LISP lfeats, LISP fname, LISP val)
{
    // Probably should restrict what can be in fname, not : would be good
    LISP lf = lfeats;
    if (lfeats == NIL)
    {
	EST_Features *f = new EST_Features;
	lf = siod(f);
    }
    feats(lf)->set_path(get_c_string(fname),val_lisp(val));
    return lf;
}

static LISP feats_get(LISP f, LISP fname)
{
    return lisp_val(feats(f)->val_path(get_c_string(fname)));
}

static LISP feats_make()
{
    EST_Features *f = new EST_Features;
    return siod(f);
}

static LISP feats_tolisp(LISP lf)
{
    return features_to_lisp(*feats(lf));
}

static LISP feats_remove(LISP lf, LISP fname)
{
    EST_Features *f = feats(lf);
    f->remove(get_c_string(fname));
    return lf;
}

static LISP feats_present(LISP lf, LISP fname)
{
    EST_Features *f = feats(lf);
    if (f->present(get_c_string(fname)))
	return truth;
    else
	return NIL;
}

EST_Features &Param()
{
    EST_Features *f = feats(siod_get_lval("Param","No Param features set"));
    return *f;
}

void siod_est_init()
{
    // add EST specific objects as user types to LISP obj
    long kind;

    // In general to add a type
    // tc_TYPENAME = siod_register_user_type("TYPENAME");
    // define above 
    //    EST_TYPENAME *get_c_TYPENAME(LISP x) and
    //    int siod_TYPENAME_p(LISP x)
    //    LISP siod_make_utt(EST_TYPENAME *x)
    // you will often also need to define 
    //    TYPENAME_free(LISP x) too if you want the contents gc'd
    // other options to the set_*_hooks functions allow you to customize
    // the object's behaviour more

    tc_utt = siod_register_user_type("Utterance");
    set_gc_hooks(tc_utt, 0, NULL,utt_mark,NULL,utt_free,NULL,&kind);

    tc_val = siod_register_user_type("Val");
    set_gc_hooks(tc_val, 0, NULL,NULL,NULL,val_free,NULL,&kind);
    set_print_hooks(tc_val,val_prin1,val_print_string);
    set_type_hooks(tc_val,NULL,val_equal);

    init_subr_2("feats.get",feats_get,
    "(feats.get FEATS FEATNAME)\n\
   Return value of FEATNAME (which may be a simple feature name or a\n\
   pathname) in FEATS.  If FEATS is nil a new feature set is created");
    init_subr_3("feats.set",feats_set,
    "(feats.set FEATS FEATNAME VALUE)\n\
   Set FEATNAME to VALUE in FEATS.");
    init_subr_2("feats.remove",feats_remove,
    "(feats.remove FEATS FEATNAME)\n\
   Remove feature names FEATNAME from FEATS.");
    init_subr_2("feats.present",feats_present,
    "(feats.present FEATS FEATNAME)\n\
   Return t is FEATNAME is present in FEATS, nil otherwise.");
    init_subr_0("feats.make",feats_make,
    "(feats.make)\n\
   Return an new empty features object.");
    init_subr_1("feats.tolisp",feats_tolisp,
    "(feats.tolisp FEATS)\n\
   Gives a lisp representation of the features, this is a debug function\n\
   and may or may not exist tomorrow.");

}

