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
 /* Represents a set of feature functions.                                */
 /*                                                                       */
 /*************************************************************************/

#include "ling_class/EST_Item.h"
#include "ling_class/EST_FeatureFunctionPackage.h"

			
static EST_Val Dummy_Func(EST_Item *) { return EST_Val(); }
static struct EST_FeatureFunctionPackage::Entry Dummy_Entry = { Dummy_Func };
template <> EST_String
EST_THash<EST_String, EST_FeatureFunctionPackage::Entry>::Dummy_Key = "DUMMY";
template <> EST_FeatureFunctionPackage::Entry
EST_THash<EST_String, EST_FeatureFunctionPackage::Entry>::Dummy_Value = Dummy_Entry;


ostream &operator << (ostream &s,
		 EST_FeatureFunctionPackage::Entry &e)
{
  (void)e;
  return s << "<<EST_FeatureFunctionPackage::Entry>>";
}

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_THash.cc"

Instantiate_TStringHash_T(EST_FeatureFunctionPackage::Entry, EST_FeatureFunctionPackage_Entry)

#endif

int operator == (const EST_FeatureFunctionPackage::Entry &e1,
		 const EST_FeatureFunctionPackage::Entry &e2)
{
return e1.func == e2.func;
}


EST_FeatureFunctionPackage::EST_FeatureFunctionPackage(const EST_String name, int n)
 : p_name(name), p_entries(n)
{
#ifdef	EST_DEBUGGING
  cerr << "initialise functionon package  " << p_name << "\n";
#endif
}

EST_FeatureFunctionPackage::EST_FeatureFunctionPackage(const char *name, int n)
  : p_name(name), p_entries(n)
{
}
	
void EST_FeatureFunctionPackage::register_func(const EST_String &name, 
					       const EST_Item_featfunc func)
{
#ifdef	EST_DEBUGGING
  cerr << "register " << p_name << "::" << name << "\n";
#endif
   if (p_entries.present(name))
	EST_warning("Feature function %s::%s redefined",
		(const char *)p_name,
		(const char *)name);

   Entry e;
   e.func=func;
   p_entries.add_item(name, e);
}

const EST_FeatureFunctionPackage::Entry &EST_FeatureFunctionPackage::lookup(const EST_String &name, int &found) const
{
  found=0;
  return p_entries.val(name, found);
}

const EST_String EST_FeatureFunctionPackage::lookup(const EST_Item_featfunc func, int &found) const
{
  EST_TStringHash<Entry>::Entries p;
  
  for(p.begin(p_entries); p; ++p)
	if (p->v.func == func)
	  {
		found=1;
		return p->k;
	  }
  found=0;
  return "";
}
