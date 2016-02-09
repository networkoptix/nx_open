/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1999                            */
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
/*                   Date   :  April 1999                                */
/*-----------------------------------------------------------------------*/
/*  Multi-linear list relations                                          */
/*                                                                       */
/*  These are to allow to linear list relations to have relations        */
/*  between one item in one list and a list of items in the other        */
/*                                                                       */
/*                                                                       */
/*  Ehh I'll explain how I do this once its done                         */
/*                                                                       */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include "ling_class/EST_Item.h"

int linked(EST_Item *from, EST_Item *to)
{
    EST_Item *i;

    for (i=link1(from); i; i=next_link(i))
	if (i == to)
	    return TRUE;

    return FALSE;
}

void add_link(EST_Item *from, EST_Item *to)
{
    EST_Item *d,*q;

    // structurally add it 
    d = from->down();
    if (!d)
	d = from->append_daughter();
    q = d->append_daughter()->append_daughter(to);
    // Also add it to the simple list of the relation so traversal works
    // append q 
    
}

#if 0 
/* nope all wrong  */
static void mls_insert_up(EST_Item *c,EST_Item *d)
{
    if (c->up() == 0)
	c->insert_above(d);
    else 
	c->up()->last()->insert_after(d);
}

static void mls_insert_below(EST_Item *c,EST_Item *d)
{
    if (c->down() == 0)
	c->insert_below(d);
    else 
	c->down()->last()->insert_after(d);
}

static void mls_linked_down(EST_Item *c,EST_Item *d)
{
    return in_list(d,c->down());
}

static void mls_linked_up(EST_Item *c,EST_Item *d)
{
    return in_list(d,c->up());
}

int link_items(EST_Relation *mlsrel,EST_Item *i1, EST_item *i2)
{
    if ((i1->in_relation(mlsrel->name())) &&
	(i2->in_relation(mlsrel->name())))
    {
	EST_error("can't link two items already in %s\n",
		  (const char *)mlsrel->name());
	return FALSE;
    }
    else if (i1->in_relation(mlsrel->name()))
    {
	EST_Item *c = mls_cluster(as(i1,mlsrel->name()));
	if (mls_linked_down(c,i1))
	    mls_insert_up(c,i2);
	else
	    mls_insert_down(c,i2);
    }
    else if (i2->in_relation(mlsrel->name()))
    {
	EST_Item *c = mls_cluster(as(i2,mlsrel->name()));
	if (mls_linked_down(c,i2))
	    mls_insert_up(c,i1);
	else
	    mls_insert_down(c,i1);
    }
    else
    {
	// neither in MLS so create new cluster
	EST_Item *c = mlsrel->append();
	mls_insert_linked_up(c,i1);
	mls_insert_linked_down(c,i1);
    }
    return TRUE;
}
#endif

void remove_link(EST_Item *from, EST_Item *to)
{
    (void)from;
    (void)to;
    
    fprintf(stderr,"remove_link not written yet\n");
}

