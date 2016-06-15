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
/*  Content part of a linguistic item, normally only referenced from     */
/*  EST_Item                                                             */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include "ling_class/EST_Item_Content.h"
#include "ling_class/EST_Item.h"
#include "EST_error.h"

void EST_Item_Content::copy(const EST_Item_Content &x)
{
    f = x.f;
    // don't copy the relations as they have relation dependencies
}

EST_Item_Content::~EST_Item_Content()
{
    if (relations.length() != 0)
    {   // Shouldn't get here,  but just in case.
	cerr << "EST_Contents: contents still referenced by Relations" << endl;
    }
}

int EST_Item_Content::unref_relation(const EST_String &relname)
{
    // Unreference this item from this relation.  Returns TRUE
    // if no one else is referencing it, FALSE otherwise
    if (this != 0)
    {
	if ((relname == "") && (relations.length() == 1))
	{   // sigh, something to with isolated EST_Items and
	    // SunCC causes a problems in exit(), so hit it with
	    // a bigger stick
	    relations.clear();
	    return TRUE;
	}
	if (relations.present(relname))
	    relations.remove_item(relname);
	else
	    printf("failed to find %s in %s at %g\n",
		   (const char *)relname,
		   (const char *)name(),
		   f.F("end",0.0));
	if (relations.length() == 0)
	    return TRUE;
    }
    return FALSE;
}

int EST_Item_Content::unref_and_delete()
{
    // Unreference from all relations and delete
    EST_Item *np;
    EST_Litem *p;

    for (p=relations.list.head(); p;)
    {
	np = ::item(relations.list(p).v);
	p=p->next();
	delete np;
    }
    // When the last relation is deleted this contents itself will be
    // delete too, from underneath us.
    return 0;
}

EST_Item_Content &EST_Item_Content::operator=(const EST_Item_Content &x)
{
    copy(x);
    return *this;
}

ostream& operator << (ostream &s, const EST_Item_Content &a)
{
    EST_Litem *p;
    s << a.name() << " ; ";
    s << a.f;
    s << "Relations";
    for (p=a.relations.list.head(); p; p = p->next())
	s << " " << a.relations.list(p).k;
    s << endl;
    return s;
}

VAL_REGISTER_CLASS_NODEL(icontent,EST_Item_Content)
