/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1997                            */
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
/*                     Date   :  November 1997                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* internal classes, methods and function used in minimization and       */
/* determinization of WFST.                                              */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include "EST_WFST.h"
#include "wfst_aux.h"

wfst_marks::wfst_marks(int x)
{
    // Set up mark table
    int i,j;

    // Triangular matrix
    p_x = x;
    p_mark_table = new char *[x];
    for (i=0; i < x; i++)
    {
	p_mark_table[i] = new char[i+1];
	for (j=0; j < i+1; j++)
	    p_mark_table[i][j] = '?';
    }
	
}

wfst_marks::~wfst_marks()
{
    int i;

    for (i=0; i < p_x; i++)
	delete [] p_mark_table[i];
    delete [] p_mark_table;
    p_mark_table = 0;
}

void wfst_marks::find_state_map(EST_IVector &state_map,int &num_new_states)
{
    // Find the mapping from old state names to new
    int i,j,new_name;
    
    state_map.resize(p_x);

    for (i=0,new_name=0; i < p_x; i++)
    {
	state_map[i] = -1;
	for (j=0; j < i; j++)
	    if (!distinguished(j,i))
	    {
		state_map[i] = state_map[j];
		break;
	    }
	if (state_map[i] == -1)
	    state_map[i] = new_name++;
    }

    num_new_states = new_name;
}

void add_assumption(int y,int z,wfst_assumes &assumptions)
{
    // Add a binding of y to z, and z to y to assumptions
    EST_Litem *p;
    int y_ok = FALSE;
    int z_ok = FALSE;

    for (p=assumptions.list.head(); p != 0; p=p->next())
    {
	if (assumptions.list(p).k == y)
	{
	    assumptions.list(p).v.append(z);
	    y_ok = TRUE;
	}
	if (assumptions.list(p).k == z)
	{
	    assumptions.list(p).v.append(y);
	    z_ok = TRUE;
	}
	if (z_ok && y_ok)
	    break;
    }
    if (!z_ok)
    {
	EST_IList b;
	b.append(y);
	assumptions.add_item(z,b);
    }
    if (!y_ok)
    {
	EST_IList b;
	b.append(z);
	assumptions.add_item(y,b);
    }
}

int equivalent_to(int y,int z,wfst_assumes &assumptions)
{
    // true if y == z or z is assumed to be equivalent to y
    EST_Litem *p,*q;

    if (y==z)
	return TRUE;
    else 
    {
	for (p=assumptions.list.head(); p != 0; p=p->next())
	{
	    if (assumptions.list(p).k == y)
	    {
		EST_IList &b = assumptions.list(p).v;
		for (q=b.head(); q != 0; q=q->next())
		    if (z == b(q))
			return TRUE;
	    }
	    if (assumptions.list(p).k == z)
	    {   
		EST_IList &b = assumptions.list(p).v;
		for (q=b.head(); q != 0; q=q->next())
		    if (y == b(q))
			return TRUE;
	    }
	}
	return FALSE;
    }
}

void mark_undistinguished(wfst_marks &marks,wfst_assumes &assumptions)
{
    EST_Litem *p, *q;

    for (p=assumptions.list.head(); p != 0; p=p->next())
    {
	int x = assumptions.list(p).k;
	EST_IList &b = assumptions.list(p).v;
	for (q=b.head(); q != 0; q=q->next())
	    marks.undistinguish(x,b(q));
    }
}

VAL_REGISTER_CLASS(wfst,EST_WFST)






