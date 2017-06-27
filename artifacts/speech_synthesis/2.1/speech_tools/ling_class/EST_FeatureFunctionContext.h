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


#ifndef __EST_FEATUREFUNCTIONCONTEXT_H__
#define __EST_FEATUREFUNCTIONCONTEXT_H__

/** A feature function context is the interface through which feature
  * functions are looked up. It is basically a list of feature function
  * packages.
  * 
  * @author Richard Caley <rjc@cstr.ed.ac.uk>
  * @version $Id: EST_FeatureFunctionContext.h,v 1.3 2004/05/04 00:00:17 awb Exp $ */

#include "ling_class/EST_FeatureFunctionPackage.h"

class ling_class_init;

class EST_FeatureFunctionContext {
private:
  EST_TList <EST_FeatureFunctionPackage *> packages;
  EST_TStringHash <EST_Item_featfunc> cache;

protected:
  static EST_FeatureFunctionContext *global;

  void add_package(EST_FeatureFunctionPackage *package);
  EST_FeatureFunctionPackage *get_package(const EST_String name) const;

  EST_String get_featfunc_name(const EST_Item_featfunc func, int &found) const;

public:
  static const EST_String separator;

  EST_FeatureFunctionContext(void);
  ~EST_FeatureFunctionContext(void);

  void clear_cache(void);
  void add_package(const EST_String name);
  bool package_included(const EST_String name) const;
  const EST_Item_featfunc get_featfunc(const EST_String name, 
				 int must=0);
  const EST_Item_featfunc get_featfunc(const EST_String package,
				 const EST_String name, 
				 int must=0);

protected:
  static void class_init(void);

  friend class ling_class_init;

  friend void EST_register_feature_function_package(const char *name, void (*init_fn)(EST_FeatureFunctionPackage &p));
  friend void register_featfunc(const EST_String &name, const EST_Item_featfunc func);
  friend const EST_Item_featfunc get_featfunc(const EST_String &name,int must);
  friend EST_String get_featname(const EST_Item_featfunc func);
};

#endif

