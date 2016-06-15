 /************************************************************************/
 /*                                                                      */
 /*                Centre for Speech Technology Research                 */
 /*                     University of Edinburgh, UK                      */
 /*                       Copyright (c) 1996,1997                        */
 /*                        All Rights Reserved.                          */
 /*                                                                      */
 /*  Permission is hereby granted, free of charge, to use and distribute */
 /*  this software and its documentation without restriction, including  */
 /*  without limitation the rights to use, copy, modify, merge, publish, */
 /*  distribute, sublicense, and/or sell copies of this work, and to     */
 /*  permit persons to whom this work is furnished to do so, subject to  */
 /*  the following conditions:                                           */
 /*   1. The code must retain the above copyright notice, this list of   */
 /*      conditions and the following disclaimer.                        */
 /*   2. Any modifications must be clearly marked as such.               */
 /*   3. Original authors' names are not deleted.                        */
 /*   4. The authors' names are not used to endorse or promote products  */
 /*      derived from this software without specific prior written       */
 /*      permission.                                                     */
 /*                                                                      */
 /*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK       */
 /*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING     */
 /*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT  */
 /*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE    */
 /*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES   */
 /*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN  */
 /*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,         */
 /*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF      */
 /*  THIS SOFTWARE.                                                      */
 /*                                                                      */
 /*************************************************************************/
 /*                                                                       */
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             */
 /* --------------------------------------------------------------------  */
 /* A complete lookup table for feature functions.                        */
 /*                                                                       */
 /*************************************************************************/

#include "ling_class/EST_Item.h"
#include "ling_class/EST_FeatureFunctionPackage.h"
#include "EST_FeatureFunctionContext.h"

#include "ling_class_init.h"

void EST_FeatureFunctionContext::class_init(void)
{
  ling_class_init::use();

  global = new EST_FeatureFunctionContext();
}

EST_FeatureFunctionContext *EST_FeatureFunctionContext::global;

const EST_String EST_FeatureFunctionContext::separator = "+";

EST_FeatureFunctionContext::EST_FeatureFunctionContext(void)
  : cache(100)
{
}

EST_FeatureFunctionContext::~EST_FeatureFunctionContext(void)
{
  EST_TList<EST_FeatureFunctionPackage *>::RwEntries p;

  for(p.begin(packages); p; ++p)
    {
      // Only the global list owns it's packages.
      if (this == global)
	delete *p;
      *p = NULL;
    }
}

EST_FeatureFunctionPackage *EST_FeatureFunctionContext::get_package(const EST_String name) const
{
  EST_TList<EST_FeatureFunctionPackage *>::Entries p;

  for(p.begin(packages); p; ++p)
    {
      EST_FeatureFunctionPackage *package = *p;
      if (package->name() == name)
	return package;
    }
  return NULL;
}

EST_String EST_FeatureFunctionContext::get_featfunc_name(const EST_Item_featfunc func, int &found) const
{
  EST_TList<EST_FeatureFunctionPackage *>::Entries p;

  found=0;

  for(p.begin(packages); p; ++p)
    {
      EST_FeatureFunctionPackage *package = *p;

      EST_String name = package->lookup(func, found);

      if (found)
	{
	  return EST_String::cat(package->name(), separator, name);
	}
    }

  found=0;
  return "";
}


void EST_FeatureFunctionContext::clear_cache(void)
{
  cache.clear();
}

void EST_FeatureFunctionContext::add_package(const EST_String name)
{
  if (this == global)
    EST_error("Attempt to add package '%s' to global list",
	      (const char *)name
	      );

  EST_FeatureFunctionPackage *package = global->get_package(name);

  if (package == NULL)
    EST_error("package '%s' not loaded",
	      (const char *)name
	      );

  packages.prepend(package);

  clear_cache();
}

void EST_FeatureFunctionContext::add_package(EST_FeatureFunctionPackage *package)
{
  packages.prepend(package);

  clear_cache();
}

bool EST_FeatureFunctionContext::package_included(const EST_String name) const
{
  return get_package(name) != NULL;
}

const EST_Item_featfunc EST_FeatureFunctionContext::get_featfunc(const EST_String name, 
							   int must)
{
  int pos, len;


  if (cache.present(name))
      return cache.val(name);

  if ((pos= name.search(separator, len, 0))>=0)
    {
	const EST_Item_featfunc func2 = 
	    get_featfunc(name.before(pos,separator.length()), 
			 name.after(pos,separator.length()), must);

       if (func2 != NULL)
	 cache.add_item(name, func2);
       return func2;
    }

  // No package name so look up directly.

  EST_TList<EST_FeatureFunctionPackage *>::Entries p;
  
  for(p.begin(packages); p; ++p)
    {
      EST_FeatureFunctionPackage *package = *p;  

      int found;

      const EST_FeatureFunctionPackage::Entry &ent = package->lookup(name, found);

      if (found)
	{
	  cache.add_item(name, ent.func);
	  return ent.func;
	}
    }

  if (must)
    EST_error("No feature function '%s'", (const char *)name);

  return NULL;
}

const EST_Item_featfunc EST_FeatureFunctionContext::get_featfunc(const EST_String pname,
							   const EST_String name, 
							   int must)
{
    EST_FeatureFunctionPackage *package = get_package(pname);

    int found;

    const EST_FeatureFunctionPackage::Entry &ent = 
	package->lookup(name, found);

    if (found)
	return ent.func;

    if (must)
	EST_error("No feature function '%s'", (const char *)name);

    return NULL;
}

static EST_Val Dummy_Func(EST_Item *) { return EST_Val(); }
template <> EST_String
EST_THash<EST_String, EST_Item_featfunc>::Dummy_Key = "DUMMY";
template <> EST_Item_featfunc
EST_THash<EST_String, EST_Item_featfunc>::Dummy_Value = Dummy_Func;
Declare_TList_T(EST_FeatureFunctionPackage *, EST_FeatureFunctionPackageP)


#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_THash.cc"

Instantiate_TStringHash(EST_Item_featfunc)

#include "../base_class/EST_TList.cc"
Instantiate_TList_T(EST_FeatureFunctionPackage *, EST_FeatureFunctionPackageP)

#endif
