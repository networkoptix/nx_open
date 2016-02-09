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
/*                   Date   :  June 1998                                 */
/*-----------------------------------------------------------------------*/
/*  Support for feature functions for EST_Items                          */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include "EST_THash.h"
#include "ling_class/EST_Item.h"
#include "ling_class/EST_Item_Content.h"
#include "ling_class/EST_Relation.h"
#include "ling_class/EST_FeatureFunctionPackage.h"
#include "EST_FeatureFunctionContext.h"

const char *error_name(EST_Item_featfunc f)
{
  (void)f;
  return "<<EST_Item_featfunc>>";
}

const EST_Item_featfunc get_featfunc(const EST_String &name,int must)
{
    const EST_Item_featfunc f = EST_FeatureFunctionContext::global->get_featfunc(name, must);

    return f;
}

void register_featfunc(const EST_String &name, const EST_Item_featfunc func)
{
    if (EST_FeatureFunctionContext::global->get_featfunc("standard", name,0) != 0)
	cerr << "item featfunc \"" << name << 
	    "\" redefined definition" << endl;

    EST_FeatureFunctionPackage *package = EST_FeatureFunctionContext::global->get_package("standard");

    package->register_func(name,func);
}

EST_String get_featname(const EST_Item_featfunc func)
{

   int found;
   EST_String name = EST_FeatureFunctionContext::global->get_featfunc_name(func, found);

   if (!found)
	EST_error("featfunc %p has no name", func);

    return name;
}

void EST_register_feature_function_package(const char *name, 
					   void (*init_fn)(EST_FeatureFunctionPackage &p))
{
  EST_FeatureFunctionPackage *package = new EST_FeatureFunctionPackage(name, 20);
  EST_FeatureFunctionContext::global->add_package(package);
  (*init_fn)(*package);
}

/* EST_Item_featfuncs may be used in EST_Vals                     */
/* EST_Item_featfuncs aren't a class so we can't use the standard */
/* registration procedure and have to explicitly write it         */
val_type val_type_featfunc = "featfunc";
const EST_Item_featfunc featfunc(const EST_Val &v)
{
    if (v.type() == val_type_featfunc)
	return (const EST_Item_featfunc)v.internal_ptr();
    else
	EST_error("val not of type val_type_featfunc");
    return NULL;
}

void val_delete_featfunc(void *v)
{   /* Function pointers can't be freed */
    (void)v;
}

void val_copy_featfunc(void *v1,void *v2)
{
    v1 = v2;
}

EST_Val est_val(const EST_Item_featfunc f)
{
    return EST_Val(val_type_featfunc,(void *)f,val_delete_featfunc);
}

#if 0
/* An example only */
EST_Val start_time(EST_Item *s)
{
    EST_Relation *r = s->relation();

    if (s == 0)
	return 0.0;
    else if ((r == 0) || (r->f("timing-style") != "segment"))
	return s->f("start");
    else
    {
	// Its to be derived.
	EST_String asrel = r->f("start-from");
	EST_Item *fl = s->first_leaf();

	if ((asrel == "0") || (asrel == s->relation_name()))
	{
	    if (prev(s))
		return prev(fl)->f("end");
	    else
		return 0.0;
	}
	else
	    return start_time(fl->as_relation(asrel));
    }
}
#endif
