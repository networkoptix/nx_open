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
/*                     Author :  Paul Taylor                             */
/*                     Date   :  July 1995                               */
/*-----------------------------------------------------------------------*/
/*                   Confusion Matrix Calculation                        */
/*                                                                       */
/*=======================================================================*/
#include <cmath>
#include "EST_multistats.h"
#include "EST_math.h"
#include "EST_types.h"

int nth(EST_String name, EST_TList<EST_String> &lex)
{
    EST_Litem *p;
    int i;

    for (i = 0, p = lex.head(); p!=0; p = p->next(), ++i)
	if (name == lex(p))
	    return i;

    cerr << "Item " << name << " not found in word list\n";
    return -1;
}

EST_FMatrix confusion(EST_StrStr_KVL &list, EST_StrList &lex)
{
    EST_FMatrix a(lex.length(), lex.length());
    EST_Litem *p;
    int n, m;
    a.fill(0.0);

    for (p = list.list.head(); p!=0; p = p->next())
    {
	m = nth(list.key(p), lex);
	n = nth(list.val(p), lex);
	if ((n != -1) && (m != -1))
	    a(m, n) = a(m, n)  + 1;
    }

    return a;
}
    
void print_confusion(const EST_FMatrix &a, EST_StrStr_KVL &list, 
		     EST_StrList &lex)
{

    int i, j;
    EST_Litem *p;
    cout << "              ";
    (void)list;
    int n = a.num_rows();

    EST_FVector row_total(n);
    EST_FVector col_total(n);
    EST_FVector correct(n);

    for (i = 0; i < n; ++i)
    {
	row_total[i] = 0.0;
	for (j = 0; j < n; ++j)
	    row_total[i] += a(i, j);
    }

    for (j = 0; j < n; ++j)
    {
	col_total[j] = 0.0;
	for (i = 0; i < n; ++i)
	    col_total[j] += a(i, j);
    }

    for (i = 0; i < n; ++i)
    {
	float rt = row_total(i);
	if (rt == 0)
	    correct[i] = 100;
	else
	    correct[i] = 100.0 * a(i, i) / rt;
    }

    for (p = lex.head(); p != 0; p = p->next())
	{
//	    cout.width(4);
//	    cout.setf(ios::right);
	    cout << lex(p).before(3) << "  ";
	}
    cout << endl;

    for (p = lex.head(), i = 0; i < n; ++i, p = p->next())
    {
	cout.width(12);
	cout << lex(p);
	for (j = 0; j < n; ++j)
	{
	    cout.width(4);
	    cout.precision(3);
	    cout.setf(ios::right);
	    cout.setf(ios::fixed, ios::floatfield);
	    cout << ( (int) a(i, j) ) << " ";
	}
	cout.width(4);
	cout << (int)row_total(i) << "   ";
	cout.width(4);
	cout.setf(ios::right);
	cout << "[" << ((int)a(i, i)) << "/" << ((int)row_total(i)) << "]";
	cout.width(12);
	cout.precision(3);
	cout.setf(ios::right);
//	cout.setf(ios::fixed, ios::floatfield);
	if (isnanf(correct(i)))
	    cout << endl;
	else
	    cout << correct(i) << endl;
    }
    cout << "            ";
    for (j = 0; j < n; ++j)
    {
	cout.width(4);
	cout << ((int)col_total(j)) << " ";
    }
    cout << endl;

    // work out total correct
    EST_FMatrix b;
    float s, t, pp;
    t = sum(a);
    b = diagonalise(a);
    s = sum(b);
    if (s == 0)
	pp = 0;
    else if (t == 0)
	pp = 100.0;  // probably can't happen
    else
	pp = 100.0 * s/t;
    cout << "total " << ((int)t) << " correct " << s << " " 
	<< pp << "%"<< endl;
}
    

