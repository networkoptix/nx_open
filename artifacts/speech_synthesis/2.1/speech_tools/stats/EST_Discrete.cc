/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1996                            */
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
/*                         Author :  Alan W Black                        */
/*                         Date   :  July 1996                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Discretes for mapping between alphabets and indexes                   */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "EST_String.h"
#include "EST_simplestats.h"

static void Discrete_val_delete_funct(void *d) { delete (int *)d; }

EST_Discrete::~EST_Discrete() 
{
    nametrie.clear(Discrete_val_delete_funct);
}

EST_Discrete::EST_Discrete(const EST_StrList &vocab)
{
    if(!init(vocab))
    {
	cerr << "WARNING from EST_Discrete ctor : invalid vocab list !";
	nametrie.clear(Discrete_val_delete_funct);
	namevector.resize(0);
    }
}

void EST_Discrete::copy(const EST_Discrete &d)
{
    int i;
    p_def_val = d.p_def_val;
    nametrie.clear(Discrete_val_delete_funct);
    namevector = d.namevector;
    
    for (i=0; i<namevector.length(); ++i)
    {
	int *t = new int;
	*t = i;
	nametrie.add(namevector(i),t);
    }
}

bool EST_Discrete::init(const EST_StrList &vocab)
{
    // initialize a new EST_Discrete to given set of names
    EST_Litem *w;
    int i,*tmp;

    p_def_val = -1;
    namevector.resize(vocab.length());
    nametrie.clear(Discrete_val_delete_funct);

    for (i=0,w=vocab.head(); w != 0; i++,w=w->next()){
	namevector[i] = vocab(w);
	tmp = new int;
	*tmp = i;

	// check for repeated items - just not allowed
	if(nametrie.lookup(vocab(w)) != NULL)
	{
	    cerr << "EST_Discrete : found repeated item '";
	    cerr << vocab(w) << "' in vocab list !" << endl;
	    return false;
	}

	nametrie.add(vocab(w),tmp);
    }
    return true;
}


bool EST_Discrete::operator ==(const EST_Discrete &d)
{
    // assume, if name vectors are the same, the stringtries
    // are too
    return (bool)(namevector == d.namevector);
}

bool EST_Discrete::operator !=(const EST_Discrete &d)
{
    return (bool)(namevector != d.namevector);
}

EST_String EST_Discrete::print_to_string(int quote)
{
    EST_String s = "";
    EST_String sep = "";
    static EST_Regex needquotes(".*[()'\";., \t\n\r].*");
    int i;
    
    for(i=0;i<length();i++)
    {
	if ((quote) && name(i).matches(needquotes))
	    s += sep + quote_string(name(i),"\"","\\",1);
	else
	    s += sep + name(i);
	sep = " ";
    }

    return s;
}

ostream& operator <<(ostream& s, const EST_Discrete &d)
{
    int i;
    for(i=0;i<d.length();i++)
	s << d.name(i) << " ";
    return s;
}

Discretes::~Discretes()
{
    int i;
 
    for (i=0; i<next_free; i++)
	delete discretes[i];
    delete discretes;
}

const int Discretes::def(const EST_StrList &vocab)
{
    //  Define discrete, increasing the size of the table if need be
    int i,pos;
    
    if (next_free == max)
    {
	EST_Discrete **new_discretes = new EST_Discrete *[max*2];
	for (i=0; i<next_free; i++)
	    new_discretes[i] = discretes[i];
	max *= 2;
	delete discretes;
	discretes = new_discretes;
    }

    discretes[next_free] = new EST_Discrete(vocab);
    pos = next_free + 10;
    next_free++;

    return pos;
}
