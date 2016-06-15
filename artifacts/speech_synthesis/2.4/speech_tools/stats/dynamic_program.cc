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
/*                   Author :  Simon King                                */
/*                   Date   :  June 1998                                 */
/*************************************************************************/

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include "EST_math.h"
#include "ling_class/EST_Utterance.h"

typedef EST_TVector<EST_Item*> EST_Item_ptr_Vector;
static EST_Item *const def_val_item_ptr = NULL;
template <> EST_Item* const *EST_Item_ptr_Vector::def_val = &def_val_item_ptr;

static EST_Item* error_return_item_ptr = NULL;
template <> EST_Item* *EST_Item_ptr_Vector::error_return = &error_return_item_ptr;

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TVector.cc"

template class EST_TVector<EST_Item*>;

#endif

typedef
float (*local_cost_function)(const EST_Item *item1,
			     const EST_Item *item2);

typedef
bool (*local_pruning_function)(const int i,
			       const int j,
			       const int max_i, 
			       const int max_j);

bool dp_sub(int i, int j,
	    const EST_Item_ptr_Vector &vr1,
	    const EST_Item_ptr_Vector &vr2,
	    EST_IMatrix &DP_path_i, EST_IMatrix &DP_path_j, 
	    local_cost_function lcf,
	    local_pruning_function lpf,
	    EST_Item *null_sym,
	    EST_FMatrix &cost);

void trace_back_and_link(int i, int j,
			 EST_Item *p1, EST_Item *p2,
			 const EST_IMatrix &DP_path_i, 
			 const EST_IMatrix &DP_path_j,
			 EST_Item *null_sym);

inline bool null_lpf(const int,const int,const int,const int)
{
    return FALSE;
}

bool dp_match(const EST_Relation &lexical,
	      const EST_Relation &surface,
	      EST_Relation &match,
	      local_cost_function lcf,
	      local_pruning_function lpf,
	      EST_Item *null_sym);


bool dp_match(const EST_Relation &lexical,
	      const EST_Relation &surface,
	      EST_Relation &match,
	      local_cost_function lcf,
	      EST_Item *null_sym)
{
    // dp without pruning

    return dp_match(lexical,surface,match,lcf,null_lpf,null_sym);

}

static float fixed_ins;
static float fixed_del;
static float fixed_sub;

float fixed_local_cost(const EST_Item *s1, const EST_Item *s2)
{
    EST_String null_sym = "nil";

    // otherwise cost is either insertion cost, or cost_matrix value
    if (s1->name() == s2->name())
	return 0;
    else
    {
	if (s1->name() == null_sym)
	    return fixed_ins;
	else if (s2->name() == null_sym)
	    return fixed_del;
	else 
	    return fixed_sub;
    }
}


bool dp_match(const EST_Relation &lexical,
	      const EST_Relation &surface,
	      EST_Relation &match,
	      float ins, float del, float sub)
{
    fixed_ins = ins;
    fixed_del = del;
    fixed_sub = sub;
    EST_Item null_sym;

    return dp_match(lexical, surface, match, fixed_local_cost, 
		    null_lpf, &null_sym);
}


bool dp_match(const EST_Relation &lexical,
	      const EST_Relation &surface,
	      EST_Relation &match,
	      local_cost_function lcf,
	      local_pruning_function lpf,
	      EST_Item *null_sym)
{
    

    // aligns lexical and surface forms using dynamic programming
    // i.e. the lexical form is transformed into the surface form
    //      by substitutions, insertions and deletions

    // makes links between associated (matching or substituted) items
    // insertions and deletions are 'left dangling'
    // links are stored in a new relation called "Match"

    // assumes that local cost computation is cheap (no caching)

    EST_IMatrix DP_path_i,DP_path_j;
    EST_Item_ptr_Vector vr1,vr2;
    EST_Item *p;
    int l1,l2,i,j;
    
    l1 = lexical.length() + 1;
    l2 = surface.length() + 1;

    vr1.resize(l1);
    vr2.resize(l2);

    // prepend null_syms
    vr1[0] = null_sym;
    vr2[0] = null_sym;

    for (p=lexical.head(),i=1; p != 0; p = p->next(),i++)
	vr1[i] = p;
    for (p=surface.head(),i=1; p != 0; p = p->next(),i++)
	vr2[i] = p;
	
    DP_path_i.resize(l1,l2);
    DP_path_j.resize(l1,l2);

/*
    cerr << "Pruning" << endl;
    for(i=0;i<l1;i++)
    {
	for(j=0;j<l2;j++)
	    if(lpf(i,j,l1,l2))
		cerr << "- ";
	    else
		cerr << "+ ";
	cerr << endl;
    }
    cerr << endl;
*/

    // end conditions : must start at (0,0) and finish at (l1-1,l2-1)
    // i.e. extreme corners of grid
    EST_FMatrix cost;
    cost.resize(vr1.length(),vr2.length());
    for(i=0;i<l1;i++)
	for(j=0;j<l2;j++)
	    cost.a_no_check(i,j) = -1; // means not computed yet

    if(!dp_sub(l1-1,l2-1,
	       vr1,vr2,
	       DP_path_i,DP_path_j,
	       lcf,lpf,null_sym,cost))
    {
	cerr << "No path found (over pruning ?)" << endl;
	return FALSE;
    }
    // make somewhere to record the relations
    //utt.create_relation("Match");
    for (p = lexical.head(); p; p = p->next())
	match.append(p);

    /*
    for(i=0;i<l1;i++)
    {
	for(j=0;j<l2;j++)
	    cerr << i << "," << j << "=[" << DP_path_i(i,j) << "," << DP_path_j(i,j) << "] ";
	cerr << endl;
    }
    cerr << endl;
*/

    trace_back_and_link(l1-1,l2-1,
			match.last(),
			surface.last(),
			DP_path_i,DP_path_j,null_sym);

    return TRUE;
}


bool dp_sub(int i, int j,
	    const EST_Item_ptr_Vector &vr1, 
	    const EST_Item_ptr_Vector &vr2,
	    EST_IMatrix &DP_path_i, EST_IMatrix &DP_path_j, 
	    local_cost_function lcf, 
	    local_pruning_function lpf, 
	    EST_Item *null_sym,
	    EST_FMatrix &cost)
{

    // the goal is to compute cost(i,j)
    
    // already done ?
    if(cost(i,j) >= 0)
	return TRUE;

    //cerr << "sub " << i << " " << j << endl;

    int best_i=-1,best_j=-1;
    float sub,ins,del;
    float best_c=MAXFLOAT;

    // prune ?
    if(lpf(i,j,vr1.length()-1,vr2.length()-1))
	return FALSE;


    // consider all paths into this point 
    // and select the best one (lowest cost)

    if(i==0)
    {
	if(j==0)
	{

	    best_i = i;
	    best_j = j;
	    best_c = lcf(null_sym,null_sym);
	}
	else
	{

	    // insert j'th item from vr2
	    if(dp_sub(i,j-1,vr1,vr2,
		      DP_path_i,DP_path_j,
		      lcf,lpf, null_sym,cost))
	    {
		best_i = i;
		best_j = j-1;
		best_c = lcf(null_sym,vr2(j)) + cost.a(i,j-1);
	    }
	    else
		return FALSE;
	}
    }

    else if(j==0)
    {
	
	// delete i'th item from vr1
	if(dp_sub(i-1,j,vr1,vr2,
		  DP_path_i,DP_path_j,
		  lcf,lpf, null_sym,cost))
	{
	    best_i = i-1;
	    best_j = j;
	    best_c = lcf(vr1(i),null_sym) + cost.a(i-1,j);
	}

    }

    // this is the simplest local constraint (i.e. no constraints !)
    // which allows unlimited consecutive insertions or deletions

    else
    {

	if(dp_sub(i-1,j-1,vr1,vr2,
		  DP_path_i,DP_path_j,
		  lcf,lpf, null_sym,cost))
	{
	    sub = 2 * lcf(vr1(i),vr2(j)) + cost(i-1,j-1);
	    if(sub < best_c)
	    {
		best_i=i-1;
		best_j=j-1;
		best_c = sub;
	    }
	}
	
	if(dp_sub(i,j-1,vr1,vr2,
		  DP_path_i,DP_path_j,
		  lcf,lpf, null_sym,cost))
	{
	    ins=lcf(null_sym,vr2(j)) + cost(i,j-1);
	    if(ins < best_c)
	    {
		best_i=i;
		best_j=j-1;
		best_c = ins;
	    }
	}
	
	if(dp_sub(i-1,j,vr1,vr2,
		  DP_path_i,DP_path_j,
		  lcf,lpf, null_sym,cost))
	{
	    del=lcf(vr1(i),null_sym) + cost(i-1,j);
	    if(del < best_c)
	    {
		best_i=i-1;
		best_j=j;
		best_c = del;
	    }
	}
    }

    cost.a(i,j) = best_c;
    DP_path_i.a_no_check(i,j) = best_i;
    DP_path_j.a_no_check(i,j) = best_j;


    //cerr << "best " << i << "," << j << " = " << best_c << endl;

    if(best_c == MAXFLOAT)
	// didn't find a better path
	return FALSE;
    else
	return TRUE;

}


void trace_back_and_link(int i, int j,
			 EST_Item *p1, EST_Item *p2,
			 const EST_IMatrix &DP_path_i, 
			 const EST_IMatrix &DP_path_j,
			 EST_Item *null_sym)
{

    //cerr << "trace " << i << " " << j << endl;


    //int i,j;
    //i=utt.relation("Lexical")->index(p1);
    //j=utt.relation("Surface")->index(p2);

    if((p1==0)&&(p2==0))
	// reached start
	return;

    if(DP_path_i(i,j) == i-1)
    {
	if(DP_path_j(i,j) == j-1)
	{
	    // match, or substitution
	    //cerr << "sub " << p1->name() << " with " << p2->name() << endl;
	    p1->append_daughter(p2);
	    p1=p1->prev();
	    p2=p2->prev();
	}
	else
	    // deletion
	    p1=p1->prev();
    }    
    else
    {
	// insertion
	// p1->append_daughter(p2); // decorative
	p2=p2->prev();
    }

    trace_back_and_link(DP_path_i(i,j), DP_path_j(i,j),
			p1,p2,
			DP_path_i,DP_path_j,
			null_sym);
}

