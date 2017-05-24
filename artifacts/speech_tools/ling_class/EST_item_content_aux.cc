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
 /* Auxiliary functions related to item_contents.                         */
 /*                                                                       */
 /*************************************************************************/

#include "ling_class/EST_item_content_aux.h"
#include "ling_class/EST_item_aux.h"
#include "ling_class/EST_Item_Content.h"
#include "EST_String.h"
#include "EST_error.h"

#include "../base_class/EST_get_function_template.h"

defineGetFunction(EST_Item_Content, f.val, EST_Val, getValI)
defineGetFunction(EST_Item_Content, f.val, EST_String, getStringI)
defineGetFunction(EST_Item_Content, f.val, float, getFloatI)
defineGetFunction(EST_Item_Content, f.val, int, getIntegerI)



EST_Val getVal(const EST_Item_Content &f,
	       const EST_String name,
	       const EST_Val &def,
	       EST_feat_status &s)
{
    if (f.relations.length() == 0)
	return getValI(f, name, def, s);
    else
    {
	EST_Litem *p;
	p = f.relations.head();
	EST_Item *i = item(f.relations.list(p).v);
	return getVal(*i, name, def, s);
    }
}


EST_String getString(const EST_Item_Content &f,
		     const EST_String name,
		     const EST_String &def,
		     EST_feat_status &s)
{
    if (f.relations.length() == 0)
	return getStringI(f, name, def, s);
    else
    {
	EST_Litem *p;
	p = f.relations.head();
	EST_Item *i = item(f.relations.list(p).v);
	return getString(*i, name, def, s);
    }
}

int getInteger(const EST_Item_Content &f,
		     const EST_String name,
		     const int &def,
		     EST_feat_status &s)
{
    if (f.relations.length() == 0)
	return getIntegerI(f, name, def, s);
    else
    {
	EST_Litem *p;
	p = f.relations.head();
	EST_Item *i = item(f.relations.list(p).v);
	return getInteger(*i, name, def, s);
    }
}

float getFloat(const EST_Item_Content &f,
		     const EST_String name,
		     const float &def,
		     EST_feat_status &s)
{
    if (f.relations.length() == 0)
	return getFloatI(f, name, def, s);
    else
    {
	EST_Litem *p;
	p = f.relations.head();
	EST_Item *i = item(f.relations.list(p).v);
	return getFloat(*i, name, def, s);
    }
}


float start(const EST_Item_Content &item_content)
{
    if (item_content.relations.length() == 0)
    {
	float v=0;
	EST_feat_status status=efs_ok;

	v = getFloat(item_content, "start", -1.0, status);

	return v;
    }
    else
    {
	EST_Litem *p;
	p = item_content.relations.head();
	EST_Item *i = item(item_content.relations.list(p).v);
	return start(*i);
    }
}

float mid(const EST_Item_Content &item_content)
{
    if (item_content.relations.length() == 0)
    {
	float v = 0;
	EST_feat_status status=efs_ok;
      
	v = getFloat(item_content, "mid", -1.0, status);
      
	if (v < 0.0)
	    v = (start(item_content)+end(item_content))/2.0;

	return v;
    }
    else
    {
	EST_Litem *p;
	p = item_content.relations.head();
	EST_Item *i = item(item_content.relations.list(p).v);
	return mid(*i);
    }
}

float time(const EST_Item_Content &item_content)
{
    if (item_content.relations.length() == 0)
    {
	float v = 0;
	EST_feat_status status=efs_ok;
      
	v = getFloat(item_content, "time", -1.0, status);
      
	if (v < 0.0)
	    v = mid(item_content);
      
	return v;
    }
    else
    {
	EST_Litem *p;
	p = item_content.relations.head();
	EST_Item *i = item(item_content.relations.list(p).v);
	return time(*i);
    }
}

float end(const EST_Item_Content &item_content)
{
    if (item_content.relations.length() == 0)
    {
	float v=0;
	EST_feat_status status=efs_ok;
      
	v = getFloat(item_content, "end", -1.0, status);
      
	return v;
    }
    else
    {
	EST_Litem *p;
	p = item_content.relations.head();
	EST_Item *i = item(item_content.relations.list(p).v);
	return end(*i);
    }
}

