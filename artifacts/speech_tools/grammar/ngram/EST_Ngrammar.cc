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
/*                     Author :  Simon King & Alan W Black               */
/*                     Date   :  February 1997                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* An EST_Ngram class for building and manipulating bigrams trigrams etc */
/*                                                                       */
/*=======================================================================*/
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <climits>
#include <cfloat>

using namespace std;

#include "EST_Ngrammar.h"
#include "EST_Pathname.h"
#include "EST_Token.h"
#include "EST_io_aux.h"


const EST_DiscreteProbDistribution PSTnullProbDistribution;
static EST_String NOVOCAB("NOVOCAB");

// **********************************************************************

EST_NgrammarState::EST_NgrammarState(const EST_NgrammarState &s)
{
    clear();
    init(s.id(),s.pdf_const());
}

EST_NgrammarState::EST_NgrammarState(const EST_NgrammarState *const s)
{
    clear();
    init(s->id(),s->pdf_const()); // copy
}

EST_NgrammarState::~EST_NgrammarState()
{
    clear();
}

void EST_NgrammarState::clear()
{
    p_id = -1;
    p_pdf.clear();
}

void EST_NgrammarState::init()
{
    p_id=-1;
    p_pdf.init();
}

void EST_NgrammarState::init(int id,EST_Discrete *d)
{
    p_id = id;
    p_pdf.init(d);
}


void EST_NgrammarState::init(int id, const EST_DiscreteProbDistribution &pdf)
{
    p_id = id;
    p_pdf = pdf; // copy it
}


ostream& operator<<(ostream& s, const EST_NgrammarState &a)
{
    s << "(" << a.id() << ": " << a.pdf_const() << " )";
    return s;
}

// **********************************************************************

EST_BackoffNgrammarState::~EST_BackoffNgrammarState()
{
    p_pdf.clear();
    children.clear();
}


void EST_BackoffNgrammarState::clear()
{
    backoff_weight=0;
    p_pdf.clear();
}

void EST_BackoffNgrammarState::init()
{
    backoff_weight=0;
    p_pdf.init();
}

void EST_BackoffNgrammarState::init(const EST_Discrete *d,int level)
{
    backoff_weight=0;
    p_level = level;
    p_pdf.init(d);
}

void EST_BackoffNgrammarState::init(const EST_DiscreteProbDistribution &pdf, int level)
{
    backoff_weight=0;
    p_level = level;
    p_pdf = pdf; // copy it
}

bool EST_BackoffNgrammarState::accumulate(const EST_StrVector &words,
					  const double count)
{

//    int i;
//    cerr << "accumulate ";
//    for(i=0;i<words.n();i++)
//    {
//	if(words.n()-1-p_level == i)
//	    cerr <<"*";
//	cerr << words(i);
//	if(words.n()-1-p_level == i)
//	    cerr <<"*";
//    }
//    cerr << endl;

    // update pdf at this node
    p_pdf.cumulate(words(words.n()-1-p_level),count);
    
    // then go down
    if (words.n()-1-p_level > 0) // not at bottom of tree
    {

	EST_BackoffNgrammarState *s;
	
	// have we got the child
	s = get_child(words(words.n()-1-p_level));
	if (s==NULL)
	    // have to extend the tree
	    s = add_child(p_pdf.get_discrete(),words);
	return s->accumulate(words,count);

    }
    else
    {
	return true;
    }
}

bool EST_BackoffNgrammarState::accumulate(const EST_IVector &words,
					  const double count)
{

//    int i;
//    cerr << "accumulate level " << p_level << " : ";
//    for(i=0;i<words.n();i++)
//    {
//	if(words.n()-1-p_level == i)
//	    cerr <<"*";
//	cerr << p_pdf.item_name(words(i));
//	if(words.n()-1-p_level == i)
//	    cerr <<"*";
//	cerr << " ";
//    }
//    cerr << endl;

    // update pdf at this node
    p_pdf.cumulate(words(words.n()-1-p_level),count);
    
    //cerr << *this << endl;
    //cerr << endl;

    // then go down
    if (words.n()-1-p_level > 0) // not at bottom of tree
    {

	EST_BackoffNgrammarState *s;
	
	// have we got the child
	s = get_child(words(words.n()-1-p_level));
	if (s==NULL)
	    // have to extend the tree
	    s = add_child(p_pdf.get_discrete(),words);

	// get pointer again in case we built more than one level
	s = get_child(words(words.n()-1-p_level));
	if (s==NULL)
	{
	    cerr << "Failed to extend tree - unknown reason !" << endl;
	    return false;
	}
	return s->accumulate(words,count);
    }
    else
    {
	return true;
    }
}


EST_BackoffNgrammarState*
EST_BackoffNgrammarState::add_child(const EST_Discrete *d,
				    const EST_StrVector &words)
{
    EST_BackoffNgrammarState *s;
    
    if (words.n()-1-p_level > 0) // more history still to go
    {
	// see if we can go down the tree
	s = get_child(words(words.n()-1-p_level));
	if (s != NULL)
	    return s->add_child(d,words);
	else
	{ 
	    // construct tree as we go
	    EST_BackoffNgrammarState *new_child = new EST_BackoffNgrammarState;
	    new_child->init(d,p_level+1);
	    children.add(words(words.n()-1-p_level), (void*)new_child);
	    return new_child->add_child(d,words);
	}
    }
    else
    {
	// this is the node we are trying to add !
	return this;
    }
}



EST_BackoffNgrammarState*
EST_BackoffNgrammarState::add_child(const EST_Discrete *d,
				    const EST_IVector &words)
{
    EST_BackoffNgrammarState *s;
    
    if (words.n()-1-p_level > 0) // more history still to go
    {
	// see if we can go down the tree
	s = get_child(words(words.n()-1-p_level));
	if (s != NULL)
	    return s->add_child(d,words);
	else
	{ 
	    // construct tree as we go
	    EST_BackoffNgrammarState *new_child = new EST_BackoffNgrammarState;
	    new_child->init(d,p_level+1);
	    children.add(p_pdf.get_discrete()->name(words(words.n()-1-p_level)), (void*)new_child);
	    return new_child->add_child(d,words);
	}
    }
    else
    {
	// this is the node we are trying to add !
	return this;
    }
}

void EST_BackoffNgrammarState::remove_child(EST_BackoffNgrammarState *child,
					    const EST_String &name)
{

    child->zap();
    // can't remove from StringTrie, but can set pointer to NULL
    children.add(name,NULL);
    delete child;
}

void EST_BackoffNgrammarState::print_freqs(ostream &os, 
					   const int order, 
					   EST_String followers)
{
    // not right - just print out, then recurse through children
    // change to use 'backoff_traverse'
    
    EST_Litem *k;
    double freq;
    EST_String name;
    for (k=p_pdf.item_start();
	 !p_pdf.item_end(k);
	 k = p_pdf.item_next(k))
    {
	p_pdf.item_freq(k,name,freq);
	EST_BackoffNgrammarState *s = ((EST_BackoffNgrammarState*)(children.lookup(name)));
	if (p_level==order-1)
	{
	    if(freq>0)
		os << name << " " << followers
		    << ": " << freq << endl;
	}
	else if (s!=NULL)
	    s->print_freqs(os,order,name+" "+followers);
	
    }
}

bool EST_BackoffNgrammarState::ngram_exists(const EST_StrVector &words,
					    const double threshold) const
{
    const EST_BackoffNgrammarState *s;
    s = get_state(words);
    if (s != NULL)
    {
	// return true for unigrams (state level 0)
	// even if freq < threshold
	return (bool)((s->level()==0) || 
		      ( s->frequency(words(0)) > threshold ));
    }
    else
      return false;
}

const EST_BackoffNgrammarState *const
EST_BackoffNgrammarState::get_state(const EST_StrVector &words) const
{
    EST_BackoffNgrammarState *s;
    if (words.n()-1-p_level > 0) // more history still to go
      {
	  s = get_child(words(words.n()-1-p_level));
	  if (s != NULL)
	  {
	      //cerr << "traversing from " << *this << endl;
	      //cerr << " to " << *s << endl << endl;
	      return s->get_state(words);
	  }
	  else
	  {
	      //cerr << "got NULL" << endl;
	      return NULL;
	  }
      }
    else
    {
	//cerr << "got " << *this << endl;
	return this;
    }
}

void EST_BackoffNgrammarState::zap()
{

    // recursively delete this state and all its children
    EST_Litem *k;
    double freq;
    EST_String name;
    for (k=p_pdf.item_start();
	 !p_pdf.item_end(k);
	 k = p_pdf.item_next(k))
    {
	p_pdf.item_freq(k,name,freq);
	EST_BackoffNgrammarState *child = get_child(name);
	if (child!=NULL) 
	    remove_child(child,name);
    }

    children.clear();
    p_pdf.clear();

}


const double EST_BackoffNgrammarState::get_backoff_weight(const EST_StrVector &words) const
{
    EST_BackoffNgrammarState *s;
    if (words.n()-1-p_level >= 0)
    {
	s = get_child(words(words.n()-1-p_level));
	if (s != NULL)
	    return s->get_backoff_weight(words);
	else
	{
	    // if there is no node, the weight would have been 1 anyway
	    //cerr << "Couldn't get weight for " << words << endl;
	    return 1;		// default
	}
    }
    
    else
    {
	// reached node
	
/*
	cerr << "got bw for ";
	for(int i=0;i<words.n();i++)
	    cerr << words(i) << " ";
	cerr << " at level " << p_level << " = " << backoff_weight << endl;
*/	
	return backoff_weight;
    }
}


bool EST_BackoffNgrammarState::set_backoff_weight(const EST_StrVector &words, const double w)
{
    EST_BackoffNgrammarState *s;
    if (words.n()-1-p_level >= 0)
    {
	s = get_child(words(words.n()-1-p_level));
	if (s != NULL)
	    return s->set_backoff_weight(words,w);
	else
	{
	    // we can't set the weight because the node
	    // doesn't exist - this is an error if the weight
	    // is not 1
	    if (w != 1)
	    {
		cerr << "Couldn't set weight for " << words 
		    << " to " << w << endl;
		return false;
	    }
	    else
		return true;	// a weight of 1 does not need storing
	}
    }
    else
    {
	// reached node
	backoff_weight = w;
	return true;
    }
}

void EST_BackoffNgrammarState::frequency_of_frequencies(EST_DVector &ff)
{
    int max=ff.n();
    EST_Litem *k;
    double freq;
    EST_String name;
    for (k=p_pdf.item_start();
	 !p_pdf.item_end(k);
	 k = p_pdf.item_next(k))
    {
	p_pdf.item_freq(k,name,freq);
	if(freq < max)
	    ff[(int)(freq+0.5)] += 1;
    }
}

ostream& operator<<(ostream& s, const EST_BackoffNgrammarState &a)
{
    s << "(backoff level:" << a.p_level
	<< " weight:" << a.backoff_weight << " " << a.pdf_const() << " )";
    
    return s;
}

// **********************************************************************

void EST_Ngrammar::default_values()
{
    p_representation = EST_Ngrammar::sparse;
    p_entry_type = EST_Ngrammar::frequencies; // by default
    sparse_representation.clear();
    allow_oov=false;
    p_num_samples = 0;
    p_num_states = 0;
    p_states = 0;
    vocab = 0;
    pred_vocab = 0;
    backoff_threshold = 1.0;
    backoff_unigram_floor_freq = 0.0;
}

EST_Ngrammar::~EST_Ngrammar()
{
    delete [] p_states;
}

void EST_Ngrammar::clear()
{
    // to do
}

bool EST_Ngrammar::init(int o, EST_Ngrammar::representation_t r, 
			const EST_StrList &wordlist)
{
    return (bool)(init_vocab(wordlist) && p_init(o,r));
}

bool EST_Ngrammar::init(int o, EST_Ngrammar::representation_t r, 
			const EST_StrList &wordlist,
			const EST_StrList &predlist)
{
    return (bool)(init_vocab(wordlist,predlist) && p_init(o,r));
}

bool EST_Ngrammar::init(int o, EST_Ngrammar::representation_t r, 
			EST_Discrete &v)
{
    vocab = &v;
    pred_vocab = vocab;
    vocab_pdf.init(pred_vocab);
    return p_init(o,r);
}

bool EST_Ngrammar::init(int o, EST_Ngrammar::representation_t r, 
			EST_Discrete &v, EST_Discrete &pv)
{
    vocab = &v;
    pred_vocab = &pv;
    vocab_pdf.init(pred_vocab);
    return p_init(o,r);
}

bool EST_Ngrammar::p_init(int o, EST_Ngrammar::representation_t r)
{
    if (o <= 0)
    {
	cerr << "EST_Ngrammar order must be > 0" << endl;
	return false;
    }
    
    p_order = o;
    p_representation = r;
    p_number_of_sentences = 0;
    
    switch(p_representation)
    {
	
    case EST_Ngrammar::sparse:
	sparse_representation.init(p_order);
	return true;
	break;
	
    case EST_Ngrammar::dense:
	return init_dense_representation();
	break;
	
    case EST_Ngrammar::backoff:
	return init_backoff_representation();
	break;
	
    default:
	cerr << "Unknown internal representation requested for EST_Ngrammar"
	    << endl;
	return false;
	break;
    }
}

bool EST_Ngrammar::init_dense_representation()
{
    // allocate a flattened  N-dimensional matrix of states
    int i;
    
    if (vocab->length() <= 0)
    {
	cerr << "EST_Ngrammar: dense_representation requires explicit vocab"
	    << endl;
	return false;
    }
    
    p_num_states = (int)pow(float(vocab->length()),float(p_order-1));
    p_states = new EST_NgrammarState[p_num_states];
    for (i=0; i < p_num_states; i++)
	p_states[i].init(i,pred_vocab);
    
    return true;
}

bool EST_Ngrammar::init_sparse_representation()
{
    
    if (vocab->length() <= 0)
    {
	cerr << "EST_Ngrammar: dense_representation requires explicit vocab"
	    << endl;
	return false;
    }
    
    p_num_states = (int)pow(float(vocab->length()),float(p_order-1));
    p_states = new EST_NgrammarState[p_num_states];
    
    return (bool)(p_states != NULL);
}

bool EST_Ngrammar::init_backoff_representation()
{
    
    // nothing much to do
    backoff_representation = new EST_BackoffNgrammarState;
    backoff_representation->init(vocab,0);
    return true;
}


const EST_StrVector &EST_Ngrammar::make_ngram_from_index(const int index) const
{
    int i,ind=index;
    EST_StrVector *ngram = new EST_StrVector;
    ngram->resize(p_order-1); // exclude last word
    
    // matches 'find_dense_state_index' so first word is most significant
    // also, cannot fill in last word
    
    for(i=p_order-2;i>=0;i--)
    {
#if defined(sun) && ! defined(__svr4__)
/* SunOS */
	int rem = ind%vocab->length();
	int quot = ind/vocab->length();
	(*ngram)[i] = wordlist_index(rem);
	ind = quot;
#else
	div_t d = div(ind,vocab->length());
	(*ngram)[i] = wordlist_index(d.rem);
	ind = d.quot;
#endif
    }
    
    return *ngram;
}




bool EST_Ngrammar::init_vocab(const EST_StrList &word_list)
{
    vocab = new EST_Discrete();
    if(!vocab->init(word_list))
	return false;
    
    pred_vocab = vocab;	// same thing in this case
    vocab_pdf.init(pred_vocab);
    
    return (bool)(vocab != NULL);
}

bool EST_Ngrammar::init_vocab(const EST_StrList &word_list,
			      const EST_StrList &pred_list)
{
    vocab = new EST_Discrete();
    if(!vocab->init(word_list))
	return false;
    pred_vocab = new EST_Discrete();
    if(!pred_vocab->init(pred_list))
	return false;
    vocab_pdf.init(pred_vocab);
    
    return (bool)(vocab != NULL);
}

bool EST_Ngrammar::check_vocab(const EST_StrList &word_list)
{
    EST_Discrete *comp_vocab = new EST_Discrete();
    
    if(!comp_vocab->init(word_list))
    {
	delete comp_vocab;
	return false;
    }
    
    if(*vocab != *comp_vocab)
    {
	delete comp_vocab;
	return false;
    }
    
    delete comp_vocab;
    return true;
}

const EST_String & EST_Ngrammar::wordlist_index(int i) const
{
    return vocab->name(i);
}

int EST_Ngrammar::predlist_index(const EST_String &word) const
{
    
    if (word=="")	// can't lookup
	return -1;
    
    int i;
    i = pred_vocab->index(word);
    if(i >= 0 )
	return i;
    
    cerr << "Word \"" << word << "\" is not in the predictee word list" << endl;
    
    if (allow_oov)
    {
	i = pred_vocab->index(OOV_MARKER);
	if(i >= 0)
	    return i;
	
	cerr << "Even " << OOV_MARKER << " is not in the predictee word list !" << endl;
    }
    
    return -1;
}


const EST_String & EST_Ngrammar::predlist_index(int i) const
{
    return pred_vocab->name(i);
}

int EST_Ngrammar::wordlist_index(const EST_String &word, const bool report) const
{
    
    if (word=="")	// can't lookup
	return -1;
    
    int i;
    i = vocab->index(word);
    if(i >= 0 )
	return i;
    
    if(report)
	cerr << "Word \"" << word << "\" is not in the word list" << endl;
    
    if (allow_oov)
    {
	i = vocab->index(OOV_MARKER);
	if(i >= 0)
	    return i;
	if(report)
	    cerr << "Even " << OOV_MARKER << " is not in the word list !" << endl;
    }
    
    return -1;
}

bool EST_Ngrammar::build(const EST_StrList &filenames,
			 const EST_String &prev,
			 const EST_String &prev_prev,
			 const EST_String &last,
			 const EST_String &input_format,
			 const EST_String &oov_mode,
			 const int mincount,
			 const int maxcount)
{
    
    p_sentence_start_marker = prev;
    p_sentence_end_marker = last;


    // when backing off, safest modes  ...
    if( (p_representation == EST_Ngrammar::backoff) &&
       (oov_mode != "skip_file") &&
       (oov_mode != "skip_sentence"))
	cerr << "Warning : building a backoff grammar" << endl
	    << "          with oov_mode '" << oov_mode 
		<< "' is not recommended !" << endl;

    if( (oov_mode != "skip_ngram") &&
       (oov_mode != "skip_sentence") &&
       (oov_mode != "skip_file") &&
       (oov_mode != "use_oov_marker")  )
    {
	cerr << "Unknown oov_mode '" << oov_mode << "'" << endl;
	return false;
    }

    if( (oov_mode == "skip_sentence") &&
       (input_format == "ngram_per_line"))
    {
	cerr << "Sorry, with input format 'ngram_per_line' you cannot " << endl
	    << " select oov_mode 'skip_sentence'" << endl;
	return false;
    }

    if(oov_mode == "use_oov_marker")
	allow_oov = true;
    else
	allow_oov = false;

    bool skip_this;
    EST_String new_filename;
    EST_Litem *p;
    for (p = filenames.head(); p; p = p->next())
    {
	cerr << "Building from " << filenames(p) << endl;

	skip_this=false;
	if( ((oov_mode == "skip_sentence") &&
	     (input_format == "sentence_per_file")) ||
	   (oov_mode == "skip_file")  )
	    skip_this = oov_preprocess(filenames(p),new_filename,
				       "skip if found");

	else if( ((oov_mode == "skip_sentence") &&
		  (input_format == "sentence_per_line")) ||
		((oov_mode == "skip_ngram") &&
		 (input_format == "ngram_per_line"))  )
	    oov_preprocess(filenames(p),new_filename,"eliminate lines");

	else
	    new_filename = filenames(p);


	if(skip_this)
	{
	    cerr << "Skipping " << filenames(p) 
		<< " (out of vocabulary words found)" << endl;
	}
	else
	{
	    
	    switch(p_representation)
	    {
	    case EST_Ngrammar::sparse:
	    {
		if (input_format != "")
		{
		    cerr << "Can't build sparse ngram from '" << input_format;
		    cerr << "' data" << endl;
		    return false;
		}
		else if (!build_sparse(new_filename,prev,prev_prev,last))
		    return false;
	    }
	    break;
	    
	case EST_Ngrammar::dense:
	    if (!build_ngram(new_filename,prev,prev_prev,last,input_format))
		return false;
	    break;
	    
	case EST_Ngrammar::backoff:
	    if (!build_ngram(new_filename,prev,prev_prev,last,input_format))
		return false;
	    break;
	    
	default:
	    cerr << "Unknown internal representation set for EST_Ngrammar"
		<< endl;
	    return false;
	    break;
	}
	}

	if((new_filename != filenames(p)) && 
	   (new_filename != "") &&
	   !delete_file(new_filename)  )
	{
	    cerr << "Warning : couldn't remove temporary file : "
		<< new_filename << endl;
	}
	
    } // loop round files

    if (p_representation == EST_Ngrammar::backoff)
	return compute_backoff_weights(mincount,maxcount);
    
    return true;
}

void EST_Ngrammar::accumulate(const EST_StrVector &words,
			      const double count)
{
    if (words.n() < p_order)
	cerr << "EST_Ngrammar::accumulate - window is too small" << endl;
    else
    {
	p_num_samples++;
	const EST_String &w = lastword(words);
	vocab_pdf.cumulate(w,count);
	
	switch(p_representation)
	{
	case EST_Ngrammar::dense:
	case EST_Ngrammar::sparse:
	    find_state(words).cumulate(w,count);
	    break;
	    
	case EST_Ngrammar::backoff:
	    backoff_representation->accumulate(words,count);
	    break;
	    
	default:
	    cerr << "EST_Ngrammar::accumulate : invalid representation !"
		<< endl;
	    break;
	}
    }			// switch
}

void EST_Ngrammar::accumulate(const EST_IVector &words,
			      const double count)
{
    
    /*
       int i;
       for(i=0;i<words.n();i++)
       {
       cerr << vocab_pdf.item_name(words(i));
       cerr << " ";
       }
       cerr << endl;
       */
    
    if (words.n() < p_order)
	cerr << "EST_Ngrammar::accumulate - window is too small" << endl;
    else
    {
	p_num_samples++;
	vocab_pdf.cumulate(words(p_order-1),count);
	
	switch(p_representation)
	{
	    
	case EST_Ngrammar::dense:
	case EST_Ngrammar::sparse:
	    find_state(words).cumulate(words(p_order-1),count);
	    break;
	    
	case EST_Ngrammar::backoff:
	    backoff_representation->accumulate(words,count);
	    break;
	    
	default:
	    cerr << "EST_Ngrammar::accumulate : invalid representation !"
		<< endl;
	    break;
	}			// switch
    }
}


bool EST_Ngrammar::ngram_exists(const EST_StrVector &words) const
{
    switch(p_representation)
    {
    case EST_Ngrammar::sparse:
	return false;
	break;
	
    case EST_Ngrammar::dense:
	return true;	// always exists !
	break;
	
    case EST_Ngrammar::backoff:
    {
	if(words.n()==1)
	    return backoff_representation->ngram_exists(words,0);
	else
	    return backoff_representation->ngram_exists(words,backoff_threshold);
    }
	break;
	
    default:
	cerr << "ngram_exists: unknown ngrammar representation" << endl;
	break;
    }
    return false;
}

bool EST_Ngrammar::ngram_exists(const EST_StrVector &words, const double threshold) const
{
    if (p_representation != EST_Ngrammar::backoff)
    {
	cerr << "Not a backoff grammar !" << endl;
	return false;
    }
    
    return backoff_representation->ngram_exists(words,threshold);
    
}


const double EST_Ngrammar::get_backoff_weight(const EST_StrVector &words) const
{
    if(p_representation == EST_Ngrammar::backoff)
	return backoff_representation->get_backoff_weight(words);
    else
    {
	cerr << "Can't get backoff weight - not a backed off ngrammar !" << endl;
	return 0;
    }
}

bool EST_Ngrammar::set_backoff_weight(const EST_StrVector &words, const double w)
{
    if(p_representation == EST_Ngrammar::backoff)
	return backoff_representation->set_backoff_weight(words,w);
    else
    {
	cerr << "Can't set backoff weight - not a backed off ngrammar !" << endl;
	return false;
    }
}


bool EST_Ngrammar::build_sparse(const EST_String &filename,
				const EST_String &prev,
				const EST_String &prev_prev,
				const EST_String &last)
{
    sparse_representation.build(filename,prev,prev_prev,last);
    return true;
}


void EST_Ngrammar::fill_window_start(EST_IVector &window, 
				     const EST_String &prev,
				     const EST_String &prev_prev) const
{
    int i;
    
    for (i=0; i<window.n()-1; i++)
	window[i] = wordlist_index(prev_prev);
    window[i++] = wordlist_index(prev);
}

void EST_Ngrammar::fill_window_start(EST_StrVector &window, 
				     const EST_String &prev,
				     const EST_String &prev_prev) const
{
    int i;
    
    for (i=0; i<window.n()-1; i++)
	window[i] = prev_prev;
    window[i++] = prev;
}

bool EST_Ngrammar::oov_preprocess(const EST_String &filename,
				  EST_String &new_filename,
				  const EST_String &what)
{
    ostream *ost=0;
    EST_TokenStream ts;
    new_filename="";
    int bad_line_count=0;
    int good_line_count=0;

    // if filename is stdin we have to make a temporary file
    // if we are eliminating lines we also need a temporary file

    // what = "skip if found" | "eliminate lines"

    bool write_out = false;
    if( (what == "eliminate lines") || (filename == "-") )
	write_out = true;

    // open the original files for reading
    if (filename == "-")
    {
	if( ts.open(stdin, FALSE) == -1)
	{
	    cerr << "EST_Ngrammar:: failed to open stdin";
	    cerr << " for reading" << endl;
	    return false;
	}
    }
    else if (ts.open(filename) == -1){
	cerr << "EST_Ngrammar: failed to open file \"" << filename 
	    << "\" for reading" << endl;
	return false; // hmmm ?
    }
    
    // open the output file if necessary
    if(write_out)
    {
	new_filename = make_tmp_filename();
	ost = new ofstream(new_filename);

	if(!(*ost))
	{
	    cerr << "Ngrammar: couldn't create temporary file \"" 
		<< new_filename << "\"" << endl;
	    new_filename="";
	    return false;
	}
    }
    else
	new_filename = filename;

    EST_String s,this_line;
    bool bad_line=false;
    while (!ts.eof())
    {
	s=ts.get().string();
	
	if (!bad_line && (s != ""))
	{
	    if(wordlist_index(s,false) < 0)
	    {

		if(what == "eliminate lines")
		{
		    bad_line=true;
		}
		else // skip file
		{
		    if(write_out) 
		    {
			// input was stdin, but we are now discarding it
			delete ost;
			if(!delete_file(new_filename))
			    cerr << "Warning : couldn't delete temporary file '"
				<< new_filename << "'" << endl;
		    }
		    new_filename="";
		    return false;
		}

	    }
	    else
		this_line += s + " ";
	}

	// write out
	if(ts.eoln())
	{
	    if(bad_line)
	    {
		bad_line_count++;
	    }
	    else
	    {
		if(write_out)
		{
		    // this_line
		    *ost << this_line << endl;
		    good_line_count++;
		}
	    }
	    bad_line=false;
	    this_line = "";
	}

    }
    cerr << "skipped " << bad_line_count << " and kept "
	<< good_line_count << " lines from file " << filename << endl;
    return true;
}

bool EST_Ngrammar::build_ngram(const EST_String &filename,
			       const EST_String &prev,
			       const EST_String &prev_prev,
			       const EST_String &last,
			       const EST_String &input_format)
{
    p_entry_type = EST_Ngrammar::frequencies;
    int bad_word=0;
    EST_String s;
    EST_TokenStream ts;
    int eoln_is_eos = FALSE;
    int sliding_window = TRUE;
    int count=0;
    clear();
    
    if ( (input_format == "") || (input_format == "sentence_per_line") )
    {
	// may do something here later
	eoln_is_eos = TRUE;
	sliding_window = TRUE;
    }
    else if (input_format == "sentence_per_file")
    {
	eoln_is_eos = FALSE;
	sliding_window = TRUE;
	p_number_of_sentences = 1;
    }
    else if(input_format == "ngram_per_line")
    {
	eoln_is_eos = FALSE;
	sliding_window = FALSE;
	p_number_of_sentences = 1;
    }
    else
    {
	cerr << "Can't build from '" << input_format << "' data" << endl;
	return false;
    }
    
    EST_IVector window(p_order);
    
    if (filename == "-")
    {
	if( ts.open(stdin, FALSE) == -1)
	{
	    cerr << "EST_Ngrammar:: failed to open stdin";
	    cerr << " for reading" << endl;
	    return false;
	}
    }
    else if (ts.open(filename) == -1){
	cerr << "EST_Ngrammar: failed to open \"" << filename 
	    << "\" for reading" << endl;
	return false;
    }
    
    // default input format is one sentence per line
    // full stops and commas etc. are taken literally
    // i.e. the user must do the preprocessing
    
    // we assume that all of prev,prev_prev,last are either
    // null or set, not a mixture of the two
    
    // at start window contains
    // [prev_prev, prev_prev, ....., prev_prev, prev]
    //  This is not added to the ngram model though
    if(sliding_window)
    {
	fill_window_start(window,prev,prev_prev);
	if (window(p_order-1) == -1)
	    bad_word = p_order;
	else if( (p_order>1) && (window(p_order-2) == -1))
	    bad_word = p_order-1;
	else
	    bad_word=0;

	if(bad_word > 0)
	  cerr << "at start : bad word at " << bad_word << endl;

    }
    while (!ts.eof())
    {
	s=ts.get().string();
	
	if (s != "")
	{
	    if(sliding_window)
	    {
		slide(window,-1);
		if (bad_word > 0)
		    bad_word--;

		window[p_order-1] = wordlist_index(s);
		if (window(p_order-1) < 0)
		{
		    cerr << "EST_Ngrammar::build_ngram " <<
			" word \"" << s << "\" is not in vocabulary, skipping"
			    << endl;
		    bad_word = p_order;
		}
		if (bad_word == 0)
		    accumulate(window);
		else
		{
		    cerr << "not accumulating : bad word at " << bad_word;
		    cerr << " window=" << window; // l<< endl;
		    bad_word--;
		}
	    }
	    else
	    {
		// not a sliding window - wait for end of line and accumulate
		if(count < p_order)
		{
		    if(count == p_order-1) // last thing in window (predictee)
			window[count++] = predlist_index(s);
		    else // not last thing (predictor)
			window[count++] = wordlist_index(s);
		    
		    if (window(count-1) < 0)
		    {
			cerr << "EST_Ngrammar::build_ngram " <<
			    " word \"" << s << "\" is not in vocabulary, skipping"
				<< endl;
			bad_word = 1;
		    }
		}
		else
		    cerr << "Too many items on line  - ignoring trailing ones !" << endl;
	    }
	}
	
	// end of sentence ?
	if(ts.eoln())
	{
       
	    if(!sliding_window)
	    {
		if((count == p_order) && bad_word == 0)
		    accumulate(window);
		count = 0;
		bad_word = 0;
	    }
	    else if (eoln_is_eos)
	    {		
		// have there been any accumulates since the last increment
		if (window(p_order-1) != wordlist_index(last))
		    p_number_of_sentences += 1;
		
		slide(window,-1);
		window[p_order-1] = wordlist_index(last);
		
		if(window(p_order-1) == -1)
		{
		    //cerr << "WARNING : skipping over bad word '" 
		    //<< last << "'" << endl;
		    bad_word = p_order;
		}
		
		if (bad_word == 0)
		    accumulate(window);
		
		fill_window_start(window,prev,prev_prev);
		
		// check for bad tags
		if (window(p_order-1) == -1)
		    bad_word = p_order;
		else if( (p_order>1) && (window(p_order-2) == -1) )
		    bad_word = p_order-1;
		else
		    bad_word=0;
	    }
	}
    }
    
    // if last accumulate was at end of sentence
    // we don't need to do the extra accumulate
    if ( sliding_window && (window(p_order-1) != wordlist_index(prev)))
    {
	
	// and finish with the ngram [.....last few words,last]
	slide(window,-1);
	window[p_order-1] = wordlist_index(last);
	
	if (window(p_order-1) == -1)
	    bad_word=p_order;
	
	if (bad_word == 0)
	{
	    accumulate(window);
	    p_number_of_sentences += 1;
	}
    }
    
    ts.close();
    
    cerr << "Accumulated " << p_number_of_sentences << " sentences." << endl;
    return true;
}


void compute_backoff_weight(EST_Ngrammar *n, EST_StrVector &ngram, void*)
{
    int i,j;
    double sum1=0,sum2=0;
    
    
    EST_StrVector new_ngram;
    new_ngram.resize(ngram.n()-1);
    for(i=0;i<new_ngram.n();i++)
	new_ngram[i] = ngram(i);
    
    
       cerr << "computing backoff w for ";
       for(i=0;i<new_ngram.n();i++)
       cerr << new_ngram(i) << " ";
       cerr << "        \r";
       
       cerr << endl;
    
    // only have backoff weights if the associated n-1 gram exists
    if (!n->ngram_exists(new_ngram))
    {
	cerr << " NONE";
	
	// if ngram really exists, but was below threshold
	// set backoff weight to 1 (always back off)
	if (n->ngram_exists(new_ngram,0))
	{
	    if(!n->set_backoff_weight(new_ngram,1))
		cerr << "WARNING : couldn't set weight !" << endl;
	    cerr << " = 1";
	}
	cerr << endl;
	return;
    }
    
    // save
    EST_String tmp = ngram(ngram.n()-1);
    
    // for each possible word in the last position
    for(j=0;j<n->get_pred_vocab_length();j++)
    {
	ngram[ngram.n()-1] = n->get_pred_vocab_word(j);
	
	for(i=0;i<ngram.n();i++)
	cerr << ngram(i) << " ";
	
	if (n->ngram_exists(ngram))
	{
	    cerr << n->probability(ngram) << " exists " << endl;
	    // if we have the complete ngram, add it to sum1
	    sum1 += n->probability(ngram);
	}
	else
	{
	    
	    // make this faster - take out of loop
	    
	    // or add the n-1 gram, excluding the first word to sum2
	    EST_StrVector tmp_ngram;
	    tmp_ngram.resize(ngram.n()-1);
	    for(i=0;i<tmp_ngram.n();i++)
		tmp_ngram[i] = ngram(i+1);

	    cerr << " unseen, P(";
	    for(i=0;i<tmp_ngram.n();i++)
		cerr << tmp_ngram(i) << " ";

	    cerr << ") = " << n->probability(tmp_ngram) << " " << endl;
	    sum2 += n->probability(tmp_ngram);
	}
    }
    
    // and fill in the backoff weight
    
    if (sum2 == 0)	// no unseen ngrams, no backing off
    {
	if(!n->set_backoff_weight(new_ngram,1))
	    cerr << "WARNING : couldn't set weight !" << endl;
	cerr << 0 << endl;
    }
    else
    {
	if (sum1 > 1)
	{
	    cerr << "NEGATIVE WEIGHT for "; 
	    for(i=0;i<new_ngram.n();i++)
		cerr << new_ngram(i) << " ";
	    cerr << endl;
	    
	    cerr << "sum1=" << sum1 << " sum2=" << sum2;
	    cerr << "  " << (1 - sum1) / sum2 << endl;
	    
	    for(j=0;j<n->get_pred_vocab_length();j++)
	    {
		ngram[ngram.n()-1] = n->get_pred_vocab_word(j);
		
				
		   if (n->ngram_exists(ngram))
		   {
		   
		   for(i=0;i<ngram.n();i++)
		   cerr << ngram(i) << " ";
		   cerr << " exists, prob = ";
		   cerr << n->probability(ngram,false,true) << endl;
		   }
		   
	    }
	    sum1 = 1;
	    sum2 = 1;	// to get a weight of 0
	}
	
	if(!n->set_backoff_weight(new_ngram, (1 - sum1) / sum2 ))
	    cerr << "WARNING : couldn't set weight !" << endl;
	
	cerr <<  "sum1=" << sum1 << " sum2=" << sum2;
	cerr << "  weight=" << (1 - sum1) / sum2 << endl;
    }
    
    // restore
    ngram[ngram.n()-1] = tmp;    
    
}

bool EST_Ngrammar::compute_backoff_weights(const int mincount,
					   const int maxcount)
{
    
    
    backoff_threshold = mincount;
    backoff_discount = new EST_DVector[p_order];
    
    int i,o;
    
    // but we must have children from the root node
    // for every unigram, since these can not be backed off
    // (must therefore be present, even if zero)
    // smoothing will fill in a floor for any zero frequencies
    
    backoff_restore_unigram_states();
    
    Good_Turing_discount(*this,maxcount,0.5);
    
    // and since some frequencies will have been set to zero
    // we have to prune away those branches of the tree
    //prune_backoff_representation();
    
    
    // now compute backoff weights, to satisfy the
    // 'probs sum to 1' condition
    
    // condition is (trigram case):
    // sum( p(w3|w1,w2) ) = 1, over all w1,w2
    
    // p(wd3|wd1,wd2)= 
    // if(trigram exists)           p_3(wd1,wd2,wd3)
    // else if(bigram w1,w2 exists) bo_wt_2(w1,w2)*p(wd3|wd2)
    // else                         p(wd3|w2)
    // and for a given wd3 they all sum to 1
    
    // so compute three sums :
    // sum(p_3(wd1,wd2,wd3)), for all w1,w2 where we have the trigram
    // sum(p(wd3|wd2)), for all w1,w2 where we have the bigram(w1,w2)
    //                  but not the trigram
    // sum(p(wd3|wd2)), for all w1,w2 where we don't have either
    
    // could probably do this recursively and more elegantly
    // but it makes my head hurt
    
    for (o=2;o<=order();o++) // for (o=1 .. .????
    {
	
	cerr << "Backing off order " << o << endl;
	//cerr << "=======================" << endl;
	
	EST_StrVector words;
	words.resize(o);
	
	// for each possible history (ngram, excluding last word)
	// compute the backoff weight
	for(i=0;i<o-1;i++)
	    words[i]="";
	words[o-1] = "!FILLED!";
	iterate(words,&compute_backoff_weight,NULL);
	
	//cerr << "=========done==========" << endl;
	
    }
    
    return true;
}


void EST_Ngrammar::backoff_restore_unigram_states()
{
    // for every item in the root pdf, look for a child
    // and make it if not present
    
    // short cut is to cumulate some zero freq bigrams
    // to force construction of states where necessary
    EST_StrVector words;
    int j;
    words.resize(2);
    words[0] = "wibble"; // doesn't matter what this is, count is 0
    for(j=0;j<get_pred_vocab_length();j++)
    {
	words[1] = get_pred_vocab_word(j);
	backoff_representation->accumulate(words,0);
    }
    
}


void EST_Ngrammar::prune_backoff_representation(EST_BackoffNgrammarState *start_state)
{
    
    if (start_state == NULL)
	start_state = backoff_representation;
    
    //cerr << "pruning state " << start_state << endl << *start_state << endl;
    
    // remove any branches with zero frequency count
    
    // find children of this state with zero freq and zap them
    EST_Litem *k;
    double freq;
    EST_String name;
    for (k=start_state->pdf_const().item_start();
	 !start_state->pdf_const().item_end(k);
	 k = start_state->pdf_const().item_next(k))
    {
	start_state->pdf_const().item_freq(k,name,freq);
	if (freq < TINY_FREQ)
	{
	    EST_BackoffNgrammarState *child = start_state->get_child(name);
	    
	    if (child!=NULL)
	    {
		//cerr << "Zapping  " << name << " : " << child->level() 
		//<< " " << child<< endl;
		start_state->remove_child(child,name);
	    }
	}
	
    }
    
    // then recurse through remaining children
    for (k=start_state->pdf_const().item_start();
	 !start_state->pdf_const().item_end(k);
	 k = start_state->pdf_const().item_next(k))
    {
	start_state->pdf_const().item_freq(k,name,freq);
	EST_BackoffNgrammarState *child = start_state->get_child(name);
	if (child!=NULL)
	{
	    //cerr << "recursing to " << name << " : " << child->level() << endl;
	    //if((child!=NULL) && (child->level() == 3))
	    //cerr << *child  << endl;
	    prune_backoff_representation(child);
	}
    }
}


ostream& operator<<(ostream& s, EST_Ngrammar &n)
{
    switch(n.p_representation)
    {
    case EST_Ngrammar::sparse:
	n.sparse_representation.print_freqs(s);
	break;
	
    case EST_Ngrammar::dense:
	s << "Dense" << endl;
	//	    s << n.dense_representation << endl;
	break;
	
    case EST_Ngrammar::backoff:
	s << "Backoff" << endl;
	s << *(n.backoff_representation) << endl;
	break;
	
    default:
	cerr << "Unknown internal representation of EST_Ngrammar : can't print"
	    << endl;
	break;
    }
    
    return s;
}

bool 
EST_Ngrammar::set_entry_type(EST_Ngrammar::entry_t new_type)
{
    if (new_type == p_entry_type)
	return true;
    
    // entry type conversion --- hmmmmm
    
    cerr << "Couldn't do entry type conversion !" << endl;
    return false;
}

bool EST_Ngrammar::sparse_to_dense()
{
    cerr << "EST_Ngrammar::sparse_to_dense() "
	<<" not implemented" << endl;
    return false;
}

bool EST_Ngrammar::dense_to_sparse()
{
    cerr << "EST_Ngrammar::dense_to_sparse()"
	<<" not implemented" << endl;
    return false;
}

int EST_Ngrammar::find_dense_state_index(const EST_IVector &words, 
					 int index) const
{
    int i,ind=0;
    for(i=0;i<p_order-1;i++)
	ind = ind*vocab->length() + words.a_no_check(i+index);
	
    return ind;
}

int EST_Ngrammar::find_next_state_id(int state, int word) const
{
    // Find a new state index from the current after moving with word
    int i,f;

    if (p_order == 1)
	return 0;
    for (f=1,i=0; i<p_order-2; i++)
	f*=vocab->length();
    return ((state%f)*vocab->length())+word;
}

EST_NgrammarState &EST_Ngrammar::find_state(const EST_StrVector &words)
{
    switch(p_representation)
    {
    case EST_Ngrammar::sparse:
	//	return p_states[sparse_representation.find_state(words)];
	return p_states[0];
	break;
	
    case EST_Ngrammar::dense:
    {
	EST_IVector tmp(words.n());
	int i;
	for(i=0;i<p_order-1;i++)
	{
	    tmp[i] = wordlist_index(words(i));
	    if (tmp(i) == -1) break;
	}
	tmp[i] = pred_vocab->index(words(i));
	if (tmp(i) == -1) break;
	return p_states[find_dense_state_index(tmp)];
    }
	break;
	
    case EST_Ngrammar::backoff:
	cerr << "find_state: not valid in backoff mode !" << endl;
	break;
	
    default:
	cerr << "find_state: unknown ngrammar representation" << endl;
	break;
    }
    
    return p_states[0];
}


const EST_NgrammarState &
EST_Ngrammar::find_state_const(const EST_StrVector &words) const
{
    switch(p_representation)
    {
    case EST_Ngrammar::sparse:
	//	return p_states[sparse_representation.find_state(words)];
	return p_states[0];
	break;
	
    case EST_Ngrammar::dense:
    {
	EST_IVector tmp(words.n());
	int i;
	for(i=0;i<p_order-1;i++)
	{
	    tmp[i] = wordlist_index(words(i));
            if (tmp(i) == -1) break;
	}
	tmp[i] = pred_vocab->index(words(i));
	if (tmp(i) == -1) break;
	return p_states[find_dense_state_index(tmp)];
    }
	break;
	
    case EST_Ngrammar::backoff:
	cerr << "find_state_const: not valid in backoff mode !" << endl;
	break;
	
    default:
	cerr << "find_state: unknown ngrammar representation" << endl;
	break;
    }
    return p_states[0];
}


EST_NgrammarState &EST_Ngrammar::find_state(const EST_IVector &words)
{
    switch(p_representation)
    {
    case EST_Ngrammar::sparse:
	//	return p_states[sparse_representation.find_state(words)];
	return p_states[0];
	break;
	
    case EST_Ngrammar::dense:
	return p_states[find_dense_state_index(words)];
	break;
	
    case EST_Ngrammar::backoff:
	cerr << "find_state: not valid in backoff mode !" << endl;
	break;
	
    default:
	cerr << "find_state: unknown ngrammar representation" << endl;
	break;
    }
    return p_states[0];
}


const EST_NgrammarState &
EST_Ngrammar::find_state_const(const EST_IVector &words) const
{
    switch(p_representation)
    {
    case EST_Ngrammar::sparse:
	//	return p_states[sparse_representation.find_state(words)];
	return p_states[0];
	break;
    case EST_Ngrammar::dense:
	return p_states[find_dense_state_index(words)];
	break;
	
    case EST_Ngrammar::backoff:
	cerr << "find_state_const: not valid in backoff mode !" << endl;
	break;
	
    default:
	cerr << "find_state: unknown ngrammar representation" << endl;
	break;
    }
    
    return p_states[0];
}


bool EST_Ngrammar::set_representation(EST_Ngrammar::representation_t new_representation)
{
    
    if (new_representation == p_representation)
	return true;
    
    if (new_representation == EST_Ngrammar::sparse)
	return sparse_to_dense();
    else if (new_representation == EST_Ngrammar::dense)
	return dense_to_sparse();
    else
    {
	cerr << "set_representation: unknown ngrammar representation" << endl;
	return FALSE;
    }
}

double EST_Ngrammar::probability(const EST_StrVector &words, bool force, const bool trace) const
{
    // Probability of this ngram
    (void)force;
    
    switch(p_representation)
    {
    case EST_Ngrammar::sparse:
    case EST_Ngrammar::dense:
	return find_state_const(words).probability(lastword(words));
	break;
	
    case EST_Ngrammar::backoff:
	return backoff_probability(words,trace);
	break;
	
    default:
	cerr << "probability: unknown ngrammar representation" << endl;
	return -1;
	break;
    }
}

double EST_Ngrammar::frequency(const EST_StrVector &words, bool force, const bool trace) const
{
    // Frequency of this ngram
    (void)force;
    
    switch(p_representation)
    {
    case EST_Ngrammar::sparse:
    case EST_Ngrammar::dense:
	return find_state_const(words).frequency(lastword(words));
	break;
	
    case EST_Ngrammar::backoff:
	return backoff_probability(words,trace); // can't do freqs
	break;
	
    default:
	cerr << "probability: unknown ngrammar representation" << endl;
	return -1;
	break;
    }
}

const EST_String &EST_Ngrammar::predict(const EST_StrVector &words,
					double *prob,int *state) const
{
    // What's the most probable word given this list of words 
    
    switch(p_representation)
    {
      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
        {
	    const EST_NgrammarState &s = find_state_const(words);
	    *state = s.id();
	    return s.most_probable(prob);
        }
	break;
    
      case EST_Ngrammar::backoff:
	state=NULL;	// there are no states per se
	return backoff_most_probable(words,prob);
	break;
    
      default:
	cerr << "probability: unknown ngrammar representation" << endl;
	return EST_String::Empty;
	break;
    }
}

const EST_String &EST_Ngrammar::predict(const EST_IVector &words,
				double *prob,int *state) const
{
    // What's the most probable word given this list of words 
    
    switch(p_representation)
    {
      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
        {
	    const EST_NgrammarState &s = find_state_const(words);
	    *state = s.id();
	    return s.most_probable(prob);
        }
	break;
    
      case EST_Ngrammar::backoff:
	cerr << "probability: IVector access to backoff not supported" << endl;
	return EST_String::Empty;
	break;
    
      default:
	cerr << "probability: unknown ngrammar representation" << endl;
	return EST_String::Empty;
	break;
    }
}

int EST_Ngrammar::find_state_id(const EST_StrVector &words) const
{
    switch(p_representation)
    {
      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
        {
	    const EST_NgrammarState &s = find_state_const(words);
	    return s.id();
	}
      default:
	cerr << "Ngrammar: representation doesn't support states" << endl;
	return 0;
	break;
    }
}

int EST_Ngrammar::find_state_id(const EST_IVector &words) const
{
    switch(p_representation)
    {
      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
        {
	    const EST_NgrammarState &s = find_state_const(words);
	    return s.id();
	}
      default:
	cerr << "Ngrammar: representation doesn't support states" << endl;
	return 0;
	break;
    }
}

EST_String EST_Ngrammar::get_vocab_word(int i) const
{
    if (vocab)
        return vocab->name(i);
    else
        return NOVOCAB;
}
 
int EST_Ngrammar::get_vocab_word(const EST_String &s) const
{
    int index = vocab->name(s);
    return index;
}

double EST_Ngrammar::reverse_probability(const EST_StrVector &words, 
					 bool force) const
{
    // Whats the probability of this ngram-1 given last word in ngram
    (void)force;
    
    switch(p_representation)
    {
      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
        {
	    const EST_NgrammarState &s = find_state_const(words);
	    // need number of occurrences of words[p_order-1]
	    return s.frequency(lastword(words))/
		vocab_pdf.frequency(lastword(words));
        }
	break;
    
      case EST_Ngrammar::backoff:
	return backoff_reverse_probability(words);
	break;
    
      default:
	cerr << "probability: unknown ngrammar representation" << endl;
	return -1;
	break;
    }
}

double EST_Ngrammar::reverse_probability(const EST_IVector &words, 
					 bool force) const
{
    // Whats the probability of this ngram-1 given last word in ngram
    (void)force;
    
    switch(p_representation)
    {
      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
        { 
	    const EST_NgrammarState &s = find_state_const(words);
	    // need number of occurrences of words[p_order-1]
	    return s.frequency(lastword(words))/
		vocab_pdf.frequency(lastword(words));
	}
	break;
    
      case EST_Ngrammar::backoff:
	cerr << "probability: reverse prob unavailable for backoff  ngram"
	    << endl;
	return -1;
	break;
    
      default:
	cerr << "probability: unknown ngrammar representation" << endl;
	return -1;
	break;
    }
}

const EST_DiscreteProbDistribution &
EST_Ngrammar::prob_dist(int state) const
{
    return p_states[state].pdf_const();
}

const EST_DiscreteProbDistribution &
EST_Ngrammar::prob_dist(const EST_StrVector &words) const
{
    
    switch(p_representation)
    {
      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
      {
	  const EST_NgrammarState &s = find_state_const(words);
	  return s.pdf_const();
      }
      break;
    
    case EST_Ngrammar::backoff:
      return backoff_prob_dist(words);
      break;
    
    default:
      cerr << "probability: unknown ngrammar representation" << endl;
      return PSTnullProbDistribution;
      break;
  }
}

const EST_DiscreteProbDistribution &
EST_Ngrammar::prob_dist(const EST_IVector &words) const
{
    
    switch(p_representation)
    {
      case EST_Ngrammar::sparse:
      case EST_Ngrammar::dense:
      {
	  const EST_NgrammarState &s = find_state_const(words);
	  return s.pdf_const();
      }
      break;
    
    case EST_Ngrammar::backoff:
      cerr << "probability: unsupport IVector access of backoff ngram" << endl;
      return PSTnullProbDistribution;
      break;
    
    default:
      cerr << "probability: unknown ngrammar representation" << endl;
      return PSTnullProbDistribution;
      break;
  }
}

EST_read_status
EST_Ngrammar::load(const EST_String &filename)
{
    
    EST_read_status r_val;
    
    //if ((r_val = load_ngram_htk_ascii(filename, *this)) != wrong_format)
    //return r_val;
    //if ((r_val = load_ngram_htk_binary(filename, *this)) != wrong_format)
    //return r_val;
    if ((r_val = load_ngram_cstr_ascii(filename, *this)) != wrong_format)
	return r_val;
    if ((r_val = load_ngram_cstr_bin(filename, *this)) != wrong_format)
	return r_val;
    
    // maybe the file is compressed ?
    EST_Pathname fname(filename);
    EST_String tmp_fname("");
    
    // crude but effective
    if (fname.extension() == GZIP_FILENAME_EXTENSION)
	tmp_fname = uncompress_file_to_temporary(filename,
						 "gzip --decompress --stdout");
    else if (fname.extension() == COMPRESS_FILENAME_EXTENSION)
	tmp_fname = uncompress_file_to_temporary(filename,"uncompress -c");
    
    if(tmp_fname != "")
    {
	r_val = load(tmp_fname);
	delete_file(tmp_fname);
	return r_val;
    }
    else
	return misc_read_error;
    
    cerr << "EST_Ngrammar::load can't determine ngrammar file type for input file " << filename << endl;
    return wrong_format;
}

EST_read_status
EST_Ngrammar::load(const EST_String &filename, const EST_StrList &wordlist)
{
    
    // for backed off grammars
    // need a wordlist to load ARPA format
    
    EST_read_status r_val;
    
    if ((r_val = load_ngram_arpa(filename, *this, wordlist)) != wrong_format)
	return r_val;
    
    // try other types, then check wordlist fits
    //if ((r_val = load_ngram_htk_ascii(filename, *this)) != wrong_format)
    //return r_val;
    //if ((r_val = load_ngram_htk_binary(filename, *this)) != wrong_format)
    //return r_val;
    if ((r_val = load_ngram_cstr_ascii(filename, *this)) != wrong_format)
    {
	if ((r_val == format_ok) && check_vocab(wordlist))
	    return r_val;
	else
	{
	    cerr << "Wordlist file does not match grammar wordlist !" << endl;
	    return misc_read_error;
	}
    }
    
    if ((r_val = load_ngram_cstr_bin(filename, *this)) != wrong_format)
    {
	if ((r_val == format_ok) && check_vocab(wordlist))
	    return r_val;
	else
	{
	    cerr << "Wordlist does not match grammar !" << endl;
	    return misc_read_error;
	}
    }
    
    
    cerr << "EST_Ngrammar::load can't determine ngrammar file type for input file " << filename << endl;
    return wrong_format;
}


void
EST_Ngrammar::make_htk_compatible()
{
    
    cerr << "EST_Ngrammar::make_htk_compatible() not written yet." << endl;
    return;
}

EST_write_status
EST_Ngrammar::save(const EST_String &filename, const EST_String type, 
		   const bool trace,double floor)
{
    
    if (type == "")
	return save(filename,"cstr_ascii",false,floor);	// choose default type
    if (type == "htk_ascii")
	return save_ngram_htk_ascii(filename, *this, floor);
    //if (type == "htk_binary")
    //return save_htk_binary(filename, *this);
    if (type == "arpa")
	return save_ngram_arpa(filename, *this);
    if (type == "cstr_ascii")
	return save_ngram_cstr_ascii(filename, *this,trace,floor);
    if (type == "cstr_bin")
	return save_ngram_cstr_bin(filename, *this, trace,floor);
    if (type == "wfst")
	return save_ngram_wfst(filename, *this);
    
    cerr << "EST_Ngrammar::save unknown output file type " << type << endl;
    return write_fail;
}

void EST_Ngrammar::iterate(EST_StrVector &words,
			   void (*function)(EST_Ngrammar *n,
					    EST_StrVector &words,
					    void *params), 
			   void *params)
{
    int i,j=-1;
    EST_String tmp;
    
    // find next item in ngram to fill in
    for(i=0;i<words.n();i++)
	if (words[i] == "")
	{
	    j=i;
	    break;
	}
    
    if (j==-1) 
    {
	// all filled in, so do the function
	(*function)(this,words,params);
	
    }
    else
    {
	
	tmp = words(j);
	if (j==p_order-1) // final position - use pred vocab
	{
	    for(i=0;i<pred_vocab->length();i++){
		words[j] = pred_vocab->name(i);
		iterate(words,function,params);
	    }
	    
	}
	else
	{
	    for(i=0;i<vocab->length();i++){
		words[j] = vocab->name(i);
		iterate(words,function,params);
	    }
	}	
	words[j] = tmp;
    }
}

void EST_Ngrammar::const_iterate(EST_StrVector &words,
				 void (*function)(const EST_Ngrammar *const n,
						  EST_StrVector &words,
						  void *params), 
				 void *params) const
{
    int i,j=-1;
    EST_String tmp;
    
    // find next item in ngram to fill in
    for(i=0;i<words.n();i++)
	if (words[i] == "")
	{
	    j=i;
	    break;
	}
    
    if (j==-1)
    {
	// all filled in, so do the function
	(*function)(this,words,params);
	
    }
    else
    {
	
	tmp = words(j);
	if (j==p_order-1) // final position - use pred vocab
	{
	    for(i=0;i<pred_vocab->length();i++){
		words[j] = pred_vocab->name(i);
		const_iterate(words,function,params);
	    }
	    
	}
	else
	{
	    for(i=0;i<vocab->length();i++){
		words[j] = vocab->name(i);
		const_iterate(words,function,params);
	    }
	}	
	words[j] = tmp;
    }
}

void EST_Ngrammar::print_freqs(ostream &os,double floor)
{
    
    if (p_representation == EST_Ngrammar::backoff)
	backoff_representation->print_freqs(os,p_order);
    else
    {
	int i,j;
        EST_Litem *k;
	EST_IVector window(p_order-1);
	
	for (i=0; i < p_num_states; i++)
	{
	    // print out each ngram : freq
	    for (k=p_states[i].pdf().item_start();
		 !p_states[i].pdf().item_end(k);
		 k = p_states[i].pdf().item_next(k))
	    {
		double freq;
		EST_String name;
		int ind = i;
		p_states[i].pdf().item_freq(k,name,freq);
		if (freq == 0)
		    freq = floor;
		if (freq > 0)
		{
		    for (j = p_order-2; j >= 0; j--)
		    {
			window[j] = ind%vocab->length();
			ind /= vocab->length();
		    }
		    for (j = 0; j < p_order-1; j++)
			os << wordlist_index(window(j)) << " ";
		    os << name << " : " << freq << endl;
		}
	    }
	}
    }
}

const EST_DiscreteProbDistribution &
EST_Ngrammar::backoff_prob_dist(const EST_StrVector &words) const
{
    // need to make this on the fly
    // by looking at all possible words in the final
    // position
    
    int i,j;
    EST_StrVector ngram;
    ngram.resize(words.n()+1);
    for(i=0;i<words.n();i++)
	ngram[i] = words(i);
    
    EST_DiscreteProbDistribution *p = new EST_DiscreteProbDistribution(pred_vocab);
    
    for(j=0;j<get_pred_vocab_length();j++)
    {
	ngram[ngram.n()-1] = get_pred_vocab_word(j);
	double tmp = backoff_probability(ngram,false);
	p->set_frequency(j,tmp); // actually probability
    }
    p->set_num_samples(1.0); // we can't do it in frequencies
    
    return *p;
}

const double EST_Ngrammar::get_backoff_discount(const int order, const double freq) const
{
    if(order > p_order)
    {
	cerr << "order too great in EST_Ngrammar::get_backoff_discount" << endl;
	return 0;
    }
    
    else if( (int)freq < backoff_discount[order-1].n())
	return backoff_discount[order-1]((int)freq);
    
    else
	return 0;
}

const double EST_Ngrammar::backoff_probability(const EST_StrVector &words,
					       const bool trace) const
{
    const EST_BackoffNgrammarState *state;
    int i;
    EST_StrVector new_ngram;
    double f=0,f2=0;
    
    
    if (trace)
    {
	cerr << "backoff_probability( ";
	for(i=0;i<words.n();i++)
	    cerr << words(i) << " ";
	cerr << ") ";
    }
    
    // are we down to the unigram ?
    if (words.n() == 1)
    {
	if (trace)
	    cerr << "unigram " << backoff_representation->probability(words(0))
		<< endl;
	
	f=backoff_representation->frequency(words(0));
	
	// it seems outrageous, but use a floor because unigram
	// probs of zero mess up backing off
	if(f>0)
	    return f / backoff_representation->pdf_const().samples();
	else
	    return backoff_unigram_floor_freq / backoff_representation->pdf_const().samples();
    }
    
    // the first n-1 words in the ngram -- deeply inefficient at the moment !
    // should pass separately
    new_ngram.resize(words.n()-1);
    for(i=0;i<new_ngram.n();i++)
	new_ngram[i] = words(i);
    
    // see if we have the ngram
    state=backoff_representation->get_state(words);
    
    if( (state != NULL) &&
       ((f=state->frequency(words(0))) > backoff_threshold) )
    {
	
	//if (trace)
	//cerr << "in state " << state << " = " << *state << endl << endl;
	
	
	// because f>0, the freq of new_ngram must be non-zero too
	
	// special case - we don't have a freq for !ENTER (or whatever it's called)
	// so use the no. of sentences used to build this grammar
	if((new_ngram(0) == p_sentence_start_marker) ||
	   (new_ngram(0) == p_sentence_end_marker) )
	{
	    f2 = p_number_of_sentences;
	    if (trace)
		cerr << "special freq used : " << f2 << endl;
	}
	else
	{
	    state=backoff_representation->get_state(new_ngram);
	    if (state == NULL)
	    {
		cerr << "Something went horribly wrong !" << endl;
		return -1;
	    }
	    // state->frequency(new_ngram(0)) is the number of times
	    // we saw new_ngram
	    
	    f2=state->frequency(new_ngram(0));
	    
	    if (trace)
	    {
		//cerr << "n-1 state is " << *state << endl;
		cerr << " using freq for " << new_ngram(0) << " of " << f2 << endl;
	    }
	}
	
	if (trace)
	{
	    
	    cerr << " ..... got (" << f << " - "
		<< get_backoff_discount(state->level()+1,f)
		    << ")/" << f2 << " = "
			<< (f - get_backoff_discount(state->level()+1,f) ) / f2
			    << endl;
	}
	return (f - get_backoff_discount(state->level()+1,f) ) / f2;
    }
    
    else			// back off
    {
	
	double bo_wt = get_backoff_weight(new_ngram);
	
	for(i=0;i<new_ngram.n();i++)
	    new_ngram[i] = words(i+1);
	
	if (trace)
	{
	    cerr << "backed off(" << bo_wt << ") to (";
	    for(i=0;i<new_ngram.n();i++)
		cerr << new_ngram(i) << " ";
	    cerr << ")  ";
	}
	
	return bo_wt * backoff_probability(new_ngram,trace);
    }
    
    // should never reach here !
    // but gcc thinks it does
    cerr << "oops !";
    return -1;
    
}


const double 
EST_Ngrammar::backoff_reverse_probability_sub(const EST_StrVector &words,
					      const EST_BackoffNgrammarState *root) const
{
    
    // so similar to backoff prob - should combine
    // to do ......
    
    const EST_BackoffNgrammarState *state;
    int i;
    EST_StrVector new_ngram;
    double f=0;
    
    /*    
       cerr << "backoff_rev_probability_sub( ";
       for(i=0;i<words.n();i++)
       cerr << words(i) << " ";
       cerr << ") ";
       */
    // are we down to the unigram ?
    if (words.n() == 1)
    {
	//cerr << "unigram " << root->probability(words(0))
	//<< endl;
	return root->probability(words(0));
    }
    
    // the first n-1 words in the ngram -- deeply inefficient at the moment !
    // should pass separately
    new_ngram.resize(words.n()-1);
    for(i=0;i<new_ngram.n();i++)
	new_ngram[i] = words(i);
    
    // see if we have the ngram
    state=root->get_state(words);
    
    
    if( (state != NULL) &&
       ((f=state->frequency(words(0))) > 0) )
    {
	// because f>0, the freq of new_ngram must be non-zero too
	state=root->get_state(new_ngram);
	if (state == NULL)
	{
	    cerr << "Something went horribly wrong !" << endl;
	    return -1;
	}
	// state->frequency(new_ngram(0)) is the number of times
	// we saw new_ngram
	//cerr << "got " << f << "/" << state->frequency(new_ngram(0)) << endl;
	return f / state->frequency(new_ngram(0));
    }
    
    else			// back off
    {
	
	double bo_wt = root->get_backoff_weight(new_ngram);
	//double bo_wt = root->get_backoff_weight(words);
	
	for(i=0;i<new_ngram.n();i++)
	    new_ngram[i] = words(i+1);
	
	//cerr << "backed off(" << bo_wt << ") ";
	return bo_wt * backoff_reverse_probability_sub(new_ngram,root);
    }
    
    // should never reach here !
    // but gcc thinks it does
    cerr << "oops !";
    return -1;
    
}

const double 
EST_Ngrammar::backoff_reverse_probability(const EST_StrVector &words) const
{
    
    // probability of words 1...n-1 , given the last word
    // easier to compute from the backoff tree structure than
    // 'normal' probability
    
    // find level 1 child corresponding to history before last word
    EST_BackoffNgrammarState *state;
    state = backoff_representation->get_child(words(words.n()-1));
    
    if(state == NULL)
    {
	// predictee isn't there so ......... ???
	return 0;
    }
    
    // now find backoff probability of words 0...n-2
    // starting from this state
    return backoff_reverse_probability_sub(words,state);
    
}

const EST_String & 
EST_Ngrammar::backoff_most_probable(const EST_StrVector &words, 
				    double *prob) const
{
    return backoff_prob_dist(words).most_probable(prob);
}

void slide(EST_IVector &v, const int l)
{
    int i;
    
    // slide elements by 'l' without  wraparound
    
    if (l==0)
	return;
    
    else if (l<0)
    {
	// slide left
	
	for(i=0;i<v.n()+l;i++)
	    v[i] = v(i-l);
	
	for(;i<v.n();i++)
	    v[i] = 0;
	
    }
    else
    {
	// slide right
	
	for(i=v.n()-1;i>=l;i--)
	    v[i] = v(i-l);
	
	for(;i>=0;i--)
	    v[i] = 0;
    }
}

void
EST_Ngrammar::backoff_traverse(EST_BackoffNgrammarState *start_state,
			       void (*function)(EST_BackoffNgrammarState *s,
						void *params),
			       void *params)
{
    
    // apply the function to this node
    function(start_state,params);
    
    // and recurse down the tree
    EST_Litem *k;
    double freq;
    EST_String name;
    for (k=start_state->pdf_const().item_start();
	 !start_state->pdf_const().item_end(k);
	 k = start_state->pdf_const().item_next(k))
    {
	start_state->pdf_const().item_freq(k,name,freq);
	EST_BackoffNgrammarState *child = start_state->get_child(name);
	if (child!=NULL)
	    backoff_traverse(child,function,params);
	
    }
}

void
EST_Ngrammar::backoff_traverse(EST_BackoffNgrammarState *start_state,
			       void (*function)(EST_BackoffNgrammarState *s,
						void *params),
			       void *params,
			       const int level)
{
    // apply the function to this node, if level is correct
    if (start_state->level() == level)
    {
	function(start_state,params);
    }
    else if (start_state->level() < level)
    {
	// and recurse down the tree if we haven't
	// reached the level yet
	EST_Litem *k;
	double freq;
	EST_String name;
	
	for (k=start_state->pdf_const().item_start();
	     !start_state->pdf_const().item_end(k);
	     k = start_state->pdf_const().item_next(k))
	{
	    start_state->pdf_const().item_freq(k,name,freq);
	    EST_BackoffNgrammarState *child = start_state->get_child(name);
	    if (child!=NULL)
		backoff_traverse(child,function,params,level);
	    
	}
	
	
    }
}
void
merge_other_grammar(EST_Ngrammar *n, EST_StrVector &ngram, void *params)
{
    
    EST_Ngrammar *other_n = (EST_Ngrammar*)((void**)params)[0];
    float *weight = (float*)((void**)params)[1];
    
    if(other_n->ngram_exists(ngram))
	n->accumulate(ngram,*weight * other_n->frequency(ngram));
    
}

bool
EST_Ngrammar::merge(EST_Ngrammar &n,float weight)
{
    EST_StrVector words;
    words.resize(p_order);
    
    void **params = new void*[2];
    params[0] = (void*)&n;
    params[1] = (void*)&weight;
    
    iterate(words,&merge_other_grammar,(void*)params);
    
    delete [] params;
    return true;
}


void slide(EST_StrVector &v, const int l)
{
    // slide elements by 'l' without  wraparound
    int i;
    
    if (l==0)
	return;
    
    else if (l<0)
    {
	// slide left
	
	for(i=0;i<v.n()+l;i++)
	    v[i] = v(i-l);
	
	for(;i<v.n();i++)
	    v[i] = "";
	
    }
    else
    {	
	// slide right
	
	for(i=v.n()-1;i>=l;i--)
	    v[i] = v(i-l);
	
	for(;i>=0;i--)
	    v[i] = "";
	
    }
}

