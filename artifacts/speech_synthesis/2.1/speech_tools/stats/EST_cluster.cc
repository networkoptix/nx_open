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
/*                      Event RFC labelling                              */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include "EST_system.h"
#include "EST_FMatrix.h"
#include "EST_cluster.h"
#include <fstream>
#include "EST_string_aux.h"
#include <cfloat>

int fn_cluster(EST_FMatrix &m, EST_CBK &cbk, float d);
int nn_cluster(EST_FMatrix &m, EST_CBK &cbk, float d);
int nn_cluster2(EST_FMatrix &m, EST_CBK &cbk, float d);
float lval(EST_FMatrix &a, float floor, int &row, int &col);
float nn_cluster3(EST_FMatrix &m, EST_CBK &cbk, EST_String method);

void init_cluster(EST_CBK &cbk, int n)
{
    int i;
    EST_TList<int> tmp;

    for (i = 0; i < n; ++i)
    {
	tmp.clear();
	tmp.append(i);
	cbk.append(tmp);
    }
}

int cluster(EST_FMatrix &m, EST_CBK &cbk, EST_TList<EST_String> &ans, EST_String method, 
		    EST_TList<EST_String> &names)
{
    float dist;
    while (cbk.length() > 1)
    {
	dist = nn_cluster3(m, cbk,  method);
	ans.append(print_codebook(cbk, dist, names));
    }
    return 0;
}

// return true if list l contains integer n
int contains(EST_TList<int> &l, int n)
{
    EST_Litem *p;

    for (p = l.head(); p != 0; p = p->next())
	if (l(p) == n)
	    return 1;

    return 0;
}

void remove_distances(EST_FMatrix &d , EST_TList<int> &group)
{
    EST_Litem *pi, *pj;
    int i, j;
    
    for (i = 0, pi = group.head(); pi != 0; pi = pi->next(), ++i)
	for (j = 0, pj = group.head(); pj != 0; pj = pj->next(), ++j)
	    d(group(pi), group(pj)) = 0.0;
}

/*EST_FMatrix remove_line(Fmatrix a, int r)
{
    int n;
    n = a.num_rows() - 1;

    Fmatrix b(n, n);

    int i, j;
    
    for (i = i2 = 0; i < n; ++i. ++i2)
	for (j = j2 =  0; j < n; ++j, ++j2)
	    if (i == r)
		;
    
*/	

void collapse(EST_FMatrix &d, EST_CBK &cbk, int row, int col)
{
    EST_Litem *pi, *pj;

    for (pi = cbk.head(); pi != 0; pi = pi->next())
	if (contains(cbk(pi), row))
	    break;

    for (pj = cbk.head(); pj != 0; pj = pj->next())
	if (contains(cbk(pj), col))
	    break;

    cbk(pi) += cbk(pj);
    remove_distances(d, cbk(pi));
    cbk.remove(pj);
}

float min(float a, float b)
{
    return (a < b) ? a: b;
}

float max(float a, float b)
{
    return (a > b) ? a: b;
}

// combine codebook groups "row" and "col" into one group and calculate a 
// new distance matrix
void collapse3(EST_FMatrix &d, EST_CBK &cbk, int row, int col, EST_String method)
{
    int i;
    EST_Litem *pi;
    EST_TList<int> v;
    float fm;

    cout << "Removing row/column " << col << endl;
    cout << "row " <<cbk.nth(row) << endl;
    cout << "col " <<cbk.nth(col) << endl;
    cbk.nth(row) += cbk.nth(col);
    cout << "row " <<cbk.nth(row) << endl;
    
    for (i = 0; i < d.num_rows(); ++i)
    {
	if ((i != row) && (i != col))
	    v.append(i);
    }

    cout << "row " << row << " col " << col << " left out " << v;
    
    for (pi = v.head(); pi != 0; pi = pi->next())
    {
	if (method == "nearest")
	    fm = min(d(row,v(pi)),d(col,v(pi)));
	else if (method == "furthest")
	    fm = max(d(row,v(pi)),d(col,v(pi)));
	else
	    fm = min(d(row,v(pi)),d(col,v(pi)));

	cout << "writing values to " << v(pi) << ", " << row << " min " 
	    << fm << endl;
	d(v(pi), row) = fm;
	d(row, v(pi)) = fm;
    }

    d = sub(d, col, col);
    cbk.remove_nth(col);
}

int nn_cluster2(EST_FMatrix &m, EST_CBK &cbk, float d)
{
    static float smallest = 0.0;
    int row=0, col=0;
    (void)d;

// Change so that all values aprt from lowest in codebook get set to
// Nan (or whatever)

    smallest = lval(m, smallest, row, col);
    cout << "smallest = " << smallest << endl;
    cout << "row = " << row << " col " << col << endl;
    collapse(m, cbk, row, col);

    for (EST_Litem *p = cbk.head(); p != 0; p = p->next())
	cout << cbk(p);
    cout << "New matrix\n" << m;

    //  cout << cbk;
    return 1;
}

float nn_cluster3(EST_FMatrix &m, EST_CBK &cbk, EST_String method)
{
    static float smallest = 0.0;
    int row=0, col=0;

// Change so that all values aprt from lowest in codebook get set to
// Nan (or whatever)

    cout << "analysing matrix\n" << m;
    smallest = lval(m, smallest, row, col);
    cout << "smallest = " << smallest << endl;
    cout << "row = " << row << " col " << col << endl;
    collapse3(m, cbk, row, col, method);

    for (EST_Litem *p = cbk.head(); p != 0; p = p->next())
	cout << cbk(p);
    cout << "New matrix\n" << m << endl << endl;

    //  cout << cbk;
    return smallest;
}

int nn_cluster(EST_FMatrix &m, EST_CBK &cbk, float d)
{
    int i;
    EST_Litem *pi, *pj;
    float smallest;
    int c = 0;
    
    i = 0;
    for (pi = cbk.head(); pi != 0; pi = pi->next(), ++i)
    {
	for (pj = pi->next(); pj != 0; pj = pj->next())
	{
	    smallest = lowestval(m, cbk(pj), cbk(pi));
	    if (smallest < d)
	    {
		cbk(pi) += cbk(pj);
		cbk(pj).clear();
	    }
	}
    }
    
    for (pi = cbk.head(); pi != 0; pi = pi->next())
    {
	if (cbk(pi).empty())
	{
	    cout << "Empty entry\n";
	    pi = cbk.remove(pi);
	    c = 1;
	}
	else
	    cout << cbk(pi);
    }
    return c;
}

int fn_cluster(EST_FMatrix &m, EST_CBK &cbk, float d)
{
    int i;
    EST_Litem *pi, *pj;
    float smallest;
    int c = 0;
    
    i = 0;
    for (pi = cbk.head(); pi != 0; pi = pi->next(), ++i)
    {
	for (pj = pi->next(); pj != 0; pj = pj->next())
	{
	    smallest = highestval(m, cbk(pj), cbk(pi));
	    if (smallest < d)
	    {
		cbk(pi) += cbk(pj);
		cbk(pj).clear();
	    }
	}
    }
    
    for (pi = cbk.head(); pi != 0; pi = pi->next())
    {
	if (cbk(pi).empty())
	{
	    cout << "Empty entry\n";
	    pi = cbk.remove(pi);
	    c = 1;
	}
	else
	    cout << cbk(pi);
    }
    return c;
}


static int sorttest(const void *a, const void *b)
{				// for use with qsort C library function.
    float *c = (float *)a;
    float *d = (float *)b;
    float res = (*c - *d);
    if (res == 0.0)
	return 0;
    return (res < 0.0) ? -1 : 1;
}

EST_FVector sort_matrix(EST_FMatrix &m)
{
    int i, j, k;
    float *v;
    int n_vals;
    
    // determine size of triangular part of matrix, excluding diagonal
    int size = m.num_rows() - 1;
    
    n_vals = 0;
    for (i = 0; i < size; ++i)
	n_vals+=(i + 1);
    
    cout<<"number of values in EST_FMatrix:" << n_vals << " size " << size << endl;
    
    v = new float[n_vals];
    
    for (i = k = 0; i < m.num_rows(); ++i)
	for (j = i + 1; j < m.num_columns(); ++j, ++k)
	{
	    cout << i << " " << j << " " << k << " " << (i * size) + k << endl;
	    v[k] = m(j, i);
	}
    
    for (i = 0; i < n_vals; ++i)
	cout << "v[" << i << "] = " << v[i] << endl;
    
    qsort(v, n_vals, sizeof(float), sorttest);
    
    EST_FVector vsort(n_vals);
    for (i = 0; i < n_vals; ++i)
	vsort[i] = v[i];
    
    return vsort;
}

EST_String print_codebook(EST_CBK &cbk, float d, EST_TList<EST_String> &names)
{
    EST_Litem *pi;
    EST_Litem *pj;
    EST_String s;
    
    s = ftoString(d) + " ";
    for (pi = cbk.head(); pi != 0; pi = pi->next())
    {
	s += "(";
	for (pj = cbk(pi).head(); pj != 0; pj = pj->next())
	{
	    if (names.empty())
		s += itoString(cbk.item(pi).item(pj));
	    else 
		s += names.nth(cbk.item(pi).item(pj));
	    if (pj->next() !=  0)
		s += "   ";
	}
	s += ") ";
    }
    return s;
}

void cluster3(EST_FMatrix &m, float d)
{
    int n = m.num_rows();
    EST_TList<int> oldcbk[12];
    
    int i, j;
    float smallest;
    
    for (i = 0; i < n; ++i)
	oldcbk[i].append(i);
    
    for (i = 0; i < n; ++i)
	cout << "n: " << i << " " << oldcbk[i] << endl;
    
    
    for (i = 0; i < n; ++i)
	for (j = i + 1; j < n; ++j)
	{
	    smallest = lowestval(m, oldcbk[j], oldcbk[i]);
	    cout << "smallest = " << smallest << " d= " << d << endl << endl;
	    if (smallest < d)
	    {
		cout << "merging " << i << " " << j << endl << endl;
		merge(oldcbk, i, j);
		n--;
	    }
	}
    
    for (i = 0; i < n; ++i)
	cout << "n: " << i << " " << oldcbk[i] << endl;
}
/*
   int cluster2(EST_FMatrix &dist, float d)
   {
   int n = dist.num_frames();
   EST_TList<int> oldcbk[12];
   EST_TList<int> newcbk[12];
   float sortval = {2.0, 3.0, 4.0, 5.0, 6.0};
   int i, j, n, m;
   EST_Litem *p;
   
   for (i = 0; i < n; ++i)
   oldcbk[i].append(i);
   
   i = 0;
   while (n > 2)
   {
   s = findval(dist, m, n, sortval[i++]);
   merge9u
   }      
   
   }
   
   float findval(EST_FMatrix &dist, int &n, int &m, float val)
   {
   int i, j;
   
   for (i = 0; i < m.num_frames(); ++i)
   for (j = 0; j < m.order(); ++j)
   if ((m.x[i][j] < (val + 0.001)) && (m.x[i][j] > (val - 0.001)))
   return;
   
   cerr << "Couldn't find value " << val << endl;
   }
   */
float lowestval(EST_FMatrix &m, EST_TList<int> &a, EST_TList<int> &b)
{
    EST_Litem *pa, *pb;
    float lowest = 100000.0;
    
    cout << "list a:" << a << "list b:" << b;
    
    for (pa = a.head(); pa != 0; pa = pa->next())
	for (pb = b.head(); pb != 0; pb = pb->next())
	{
	    //      cout << "m:" << a(pa) << " " << b(pb) << " " << m.x[a(pa)][b(pb)] << endl;
	    if (m(a(pa), b(pb)) < lowest)
		lowest = m(a(pa), b(pb));
	}
    //  cout << "lowest " << lowest << endl;
    return lowest;
}

// find the lowest value in matrix a above floor, and return it. Also
// set row and column to be the indices of it.
float lval(EST_FMatrix &a, float floor, int &row, int &col)
{
    int i, j;
    float lowest = FLT_MAX;
    
    for (i = 0; i < a.num_rows(); ++i)
	for (j = 0; j < a.num_rows(); ++j)
	    if ((a(i, j) < lowest) && (a(i, j) > floor))
	    {
		lowest = a(i, j);
		row = i;
		col = j;
	    }
    return lowest;
}

float highestval(EST_FMatrix &m, EST_TList<int> &a, EST_TList<int> &b)
{
    EST_Litem *pa, *pb;
    float h = 0.0;
    
    cout << "list a:" << a << "list b:" << b;
    
    for (pa = a.head(); pa != 0; pa = pa->next())
	for (pb = b.head(); pb != 0; pb = pb->next())
	{
	    if (m(a(pa), b(pb)) > h)
		h = m(a(pa), b(pb));
	}
    //  cout << "lowest " << lowest << endl;
    return h;
}
/*
   float nearest(EST_FMatrix &m, EST_TList<int> &cbk)
   {
   EST_Litem *p;
   float lowest = 100000.0;
   
   for (p = cbk.head(); p != 0; p = p->next())
   {
   cout << "cbk(p) " << cbk(p) << endl;
   if (cbk(p) < lowest)
   lowest = cbk(p);
   }
   
   cout << "lowest = " << lowest << endl;
   return lowest;
   }
   */
void merge(EST_TList<int> cbk[], int i, int j)
{
    EST_Litem *p;
    
    for (p = cbk[j].head(); p != 0; p = p->next())
	cbk[i].append(cbk[j].item(p));
    
    cbk[j].clear();
}

int load_names(EST_String file, EST_TList<EST_String> &names)
{
    char inbuf[1000];
    EST_String tmpstr;
    
    ifstream inf(file);
    if (!inf) cerr << "Can't open names file " << file << endl;
    
    while(inf.getline(inbuf, 1000))
    {
	tmpstr = inbuf;
	names.append(tmpstr);
    }
    return 0;
}

/*int merge(EST_TList<int> &a, EST_TList<int> &b)
  {
  EST_TList<int> newgroup;
  EST_Litem *p;
  
  for (p = cbk[j].head(); p != 0; p = p->next())
  cbk[i].append(cbk[j].item(p));
  
  cbk[j].clear();
  }
  */

