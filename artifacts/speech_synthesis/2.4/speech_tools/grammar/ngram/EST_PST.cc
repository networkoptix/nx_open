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
/*             A general class for PredictionSuffixTrees                 */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST_String.h"
#include "EST_types.h"
#include "EST_Token.h"
#include "EST_PST.h"
#include "EST_multistats.h"

VAL_REGISTER_CLASS(pstnode,EST_PredictionSuffixTree_tree_node)

// Out of vocabulary identifier
const EST_String PredictionSuffixTree_oov("_OOV_");
const EST_DiscreteProbDistribution PSTnullProbDistribution;

void slide(EST_IVector &i, const int l);
void slide(EST_StrVector &i, const int l);

EST_PredictionSuffixTree_tree_node::~EST_PredictionSuffixTree_tree_node()
{
}

void 
EST_PredictionSuffixTree_tree_node::print_freqs(ostream &os)
{
    // Print out path and frequency for each leaf

    if (p_level == 0)
    {
	// Base -- print from pd 
	EST_String s;
	double freq;
        EST_Litem *i;
	for (i = pd.item_start(); 
	     !pd.item_end(i); 
	     i=pd.item_next(i))
	{
	    pd.item_freq(i,s,freq);
	    os << get_path() << " " << s << " : " << freq << endl;
	}
    }
    else
    {
	EST_Features::Entries t;
	for (t.begin(nodes); t; t++)
	    pstnode(t->v)->print_freqs(os);
    }
}

void 
EST_PredictionSuffixTree_tree_node::print_probs(ostream &os)
{
    // Print out path and probability distributions for node above leafs

    if (p_level == 0)
    {
	// Base -- print from pd 
	EST_String s;
	double prob;
	os << get_path() << " :";
	for (EST_Litem *i = pd.item_start(); !pd.item_end(i) ; i=pd.item_next(i))
	{
	    pd.item_prob(i,s,prob);
	    os << " " << s << " " << prob;
	}
	os << endl;
    }
    else
    {
	EST_Features::Entries t;
	for (t.begin(nodes); t; t++)
	    pstnode(t->v)->print_probs(os);
    }
}

const EST_String &
EST_PredictionSuffixTree_tree_node::most_probable(double *prob) const
{
    // Find most probable symbol in this node 
    return pd.most_probable(prob);
}

EST_PredictionSuffixTree::EST_PredictionSuffixTree(void)
{
    p_order = 0;
    num_states = 0;
    nodes = 0;
    pd = 0;
}

void 
EST_PredictionSuffixTree::init(const int order)
{
    p_order = order;
    num_states = 0;
    nodes = new EST_PredictionSuffixTree_tree_node;
    nodes->set_level(order-1);
    pd = new EST_DiscreteProbDistribution;
}

EST_PredictionSuffixTree::~EST_PredictionSuffixTree()
{
    delete nodes;
    delete pd;
}

void 
EST_PredictionSuffixTree::clear(void)
{
    delete nodes;
    delete pd;
    pd = 0;
}

const EST_DiscreteProbDistribution &
EST_PredictionSuffixTree::p_prob_dist(EST_PredictionSuffixTree_tree_node *node, 
				      const EST_StrVector &words,
				      const int index) const
{
    // Return the probability distribution for this EST_PredictionSuffixTree context

    if (words.n() == index+1){
	return node->prob_dist();
    }else
    {
	EST_PredictionSuffixTree_tree_node *next;
	EST_PredictionSuffixTree_tree_node *n=0;
	next = pstnode(node->nodes.f(words(index),est_val(n)));
	if (next == 0)
	{
	    //cerr << "EST_PredictionSuffixTree: "
	    //<< "no EST_PredictionSuffixTree probabilities for context \n";
	    return PSTnullProbDistribution;
	}
	return p_prob_dist(next,words,index+1);
    }
}


void 
EST_PredictionSuffixTree::accumulate(const EST_StrVector &words,
				     const double count,
				     const int index)
{

/*
    cerr << "accumulate ";
    for (int i=0; i < p_order-1; i++)
	cerr << words(i+index) << " ";
    cerr << count << endl;
    */

    if (words.n()+index < p_order)
	cerr << "EST_PredictionSuffixTree: accumlating window is wtoo small"
	     << endl;
    else
    {
	pd->cumulate(words(p_order-1+index),count); // a little extra book-keeping
	p_accumulate(nodes,words,count,index);
    }
}

void 
EST_PredictionSuffixTree::p_accumulate(EST_PredictionSuffixTree_tree_node *node,
				       const EST_StrVector &words,
				       double count,
				       const int index)
{
    /* Expand tree with new gram */
    if (words.n() == index+1)
	{
	    if (node->prob_dist().samples() == 0) // A new terminal node
		node->set_state(num_states++);
	    node->cumulate(words(index),count);
	}
    else 
    {
	EST_PredictionSuffixTree_tree_node *next;
	EST_PredictionSuffixTree_tree_node *n = 0;
	next = pstnode(node->nodes.f(words(index),est_val(n)));
	if (next == 0)
	{
	    next = new EST_PredictionSuffixTree_tree_node;
	    if (node->get_path() == "")
		next->set_path(words(index));
	    else
	    {
		next->set_path(node->get_path()+" "+ words(index));
	    }
	    next->set_level(node->get_level()-1);
	    node->nodes.set_val(words(index),est_val(next));
	}
	p_accumulate(next,words,count,index+1);
    }
}

double 
EST_PredictionSuffixTree::rev_prob(const EST_StrVector &words) const
{
    // Reverse probability.  What is prob of this context given predictee
    const EST_DiscreteProbDistribution &pg = prob_dist(words);
    double d1 = pg.frequency(words(order()-1)); 
    double d2 = pd->frequency(words(order()-1));
    double d3 = d1/d2;
    return d3;
}

double 
EST_PredictionSuffixTree::rev_prob(const EST_StrVector &words,
				   const EST_DiscreteProbDistribution &pg) const
{
    // Reverse probability.  What is prob of this context given predictee
    return (double)pg.frequency(words(order()-1)) / 
	pd->frequency(words(order()-1));
}

const 
EST_String &EST_PredictionSuffixTree::predict(const EST_StrVector &words) const
{
    double p;
    int state;
    return ppredict(nodes,words,&p,&state);
}

const 
EST_String &EST_PredictionSuffixTree::predict(const EST_StrVector &words,double *p) const
{
    int state;
    return ppredict(nodes,words,p,&state);
}

const 
EST_String &EST_PredictionSuffixTree::predict(const EST_StrVector &words,double *p, int *state) const
{
    return ppredict(nodes,words,p,state);
}

const 
EST_String &EST_PredictionSuffixTree::ppredict(EST_PredictionSuffixTree_tree_node *node,
					       const EST_StrVector &words,
					       double *p,int *state,
					       const int index) const
{
    /* Given the context whats the most probably symbol (or OOV) */
    if (words.n() == index+1)
    {
	*state = node->get_state();
	return node->most_probable(p);
    }
    else 
    {
	EST_PredictionSuffixTree_tree_node *next;
	EST_PredictionSuffixTree_tree_node *n=0;
	next = pstnode(node->nodes.f(words(index),est_val(n)));
	if (next == 0)
	{
	    *p = 0.0;
	    *state = 0;       // No not really acceptable
	    return PredictionSuffixTree_oov; /* utterly improbable -- i.e. not in EST_PredictionSuffixTree */
	}
	else
	    return ppredict(next,words,p,state,index+1);
    }
}
	    
void 
EST_PredictionSuffixTree::print_freqs(ostream &os)
{
    // Print a list of all PredictionSuffixTrees with their frequencies

    os << "EST_PredictionSuffixTree order=" << p_order << endl;
    nodes->print_freqs(os);
    
}

void 
EST_PredictionSuffixTree::print_probs(ostream &os)
{
    // Print a list of all grams(-last) with their probability distributions

    os << "EST_PredictionSuffixTree " << p_order << endl;
    nodes->print_probs(os);

}

int 
EST_PredictionSuffixTree::save(const EST_String filename, const EST_PredictionSuffixTree::EST_filetype type)
{
    // Save an EST_PredictionSuffixTree to file -- actual the grams and frequencies
    (void)type;

    if (filename == "-")
	print_freqs(cout);
    else
    {
	ofstream os(filename);
	print_freqs(os);
    }
    return 0;
}

int 
EST_PredictionSuffixTree::load(const EST_String filename)
{
    // Load an EST_PredictionSuffixTree for given file
    EST_TokenStream ts;
    int i,order,freq;

    clear();
    if (ts.open(filename) != 0)
    {
	cerr << "EST_PredictionSuffixTree: failed to open \"" << filename << "\" for reading\n";
	return -1;
    }
    ts.set_SingleCharSymbols(":");

    if (ts.get() != "EST_PredictionSuffixTree")  // magic number 
    {
	cerr << "EST_PredictionSuffixTree: file \"" << filename << "\" not an EST_PredictionSuffixTree\n";
	return -1;
    }

    order = atoi(ts.get().string());
    if ((order < 1) || (order > 10))
    {
	cerr << "EST_PredictionSuffixTree: file \"" << filename << "\" has bad order\n";
	return -1;
    }
    init(order);
    //EST_String const* *window = new EST_String const*[order+1];
    EST_StrVector window(order);
    for (i=0; i<p_order; i++)
	window[i] = "";

    while (!ts.eof())
    {
	slide(window,-1);
	window[p_order-1] = ts.get().string();
	if (ts.get() != ":")
	{
	    cerr << "EST_PredictionSuffixTree: file \"" << filename << "\" missed parsed line ";
	    cerr << ts.linenum() << " near EST_PredictionSuffixTree\n";
	    for (i=0; i < order; i++)
		cout << " " << window(i);
	    cout << endl;
	}

	freq = atoi(ts.get().string());
	accumulate(window,freq);
    }

    return 0;
}

void 
EST_PredictionSuffixTree::build(const EST_String filename,
				const EST_String prev,
				const EST_String prev_prev,
				const EST_String last)
{
    EST_TokenStream ts;
    int i;

    if (filename == "-")
	ts.open(stdin, FALSE);
    else if (ts.open(filename) == -1)
	return;

    //EST_String const* *window = new EST_String const*[p_order+1];
    EST_StrVector window(p_order);
    for (i=0; i<p_order-1; i++)
	window[i] = prev_prev;
    window[p_order-1] = prev;

    accumulate(window,1);

    //window[p_order] = 0; // end of array marker

    while (!ts.eof())
    {
	slide(window,-1);
	window[p_order-1] = ts.get().string();
	accumulate(window,1);
    }

    // and finish off

    slide(window,-1);
    window[p_order-1] = last;
    accumulate(window,1);
    
}

void 
EST_PredictionSuffixTree::build(const EST_StrList &input)
{

    // knackered - fix


    // adapted from build(..) above
    // but could be made more efficient

    int i;
    //EST_String const* *window = new EST_String const *[p_order+1];
    EST_StrVector window(p_order);
    for (i=0; i<p_order; i++)
	window[i] = "";

    EST_Litem *i_ptr;
    for(i_ptr=input.head();i_ptr!=NULL;i_ptr=i_ptr->next())
    {
	slide(window,-1);
	window[p_order-1] = input(i_ptr);
	accumulate(window,1);
    }
}


void 
EST_PredictionSuffixTree::test(const EST_String filename)
{
    // Run the current EST_PredictionSuffixTree over the given file name and generate
    // statistics of its prediction power for the contents of that 
    // file.  Use the confusion function
    EST_StrStr_KVL pairs;
    EST_StrList lex;
    EST_TokenStream ts;
    int i;

    if (filename == "-")
	ts.open(stdin, FALSE);
    else if (ts.open(filename) == -1)
	return;

    /* The top level nodes list will always contain all the tokens */
    /* Build the lexicon for confusion matrix                      */
    EST_Features::Entries p;
    for (p.begin(nodes->nodes); p; p++)
	lex.append(p->k);
    lex.append("_OOV_");

    EST_StrVector window(p_order);
    //EST_String const* *window = new EST_String const*[p_order+1];
    for (i=0; i<p_order; i++)
	window[i] = "";
    double e=0;
    int num_tsamples = 0;

    while (!ts.eof())
    {
	slide(window,-1);
	window[p_order-1] = ts.get().string();
	const EST_DiscreteProbDistribution &pdist = prob_dist(window);
	e += pdist.entropy();
	num_tsamples++;
	// cumulate real and predicted 
	pairs.add_item(window(p_order-1),predict(window),1);  
    }

    const EST_FMatrix &m = confusion(pairs,lex);
    print_confusion(m,pairs,lex);
    cout << "Mean entropy (?) is " << e/num_tsamples << endl;


}
    
