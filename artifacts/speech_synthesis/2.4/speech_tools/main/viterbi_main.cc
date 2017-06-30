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
/*                 Authors:  Alan W Black and Simon King                 */
/*                 Date   :  January 1997                                */
/*-----------------------------------------------------------------------*/
/*  A simple use of the Viterbi decoder                                  */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include "EST.h"

EST_read_status load_TList_of_StrVector(EST_TList<EST_StrVector> &w,
					const EST_String &filename,
					const int vec_len);

static void print_results(EST_Relation &wstream);
static bool do_search(EST_Relation &wstream);
static EST_VTPath *vit_npath(EST_VTPath *p,EST_VTCandidate *c,EST_Features &f);
static EST_VTCandidate *vit_candlist(EST_Item *s,EST_Features &f);
static void top_n_candidates(EST_VTCandidate* &all_c);
static void load_vocab(const EST_String &vfile);

static void add_word(EST_Relation &w, const EST_String &word, int pos);

static void load_wstream(const EST_String &filename,
			 const EST_String &vfile,
			 EST_Relation &w,
			 EST_Track &obs);

static void load_given(const EST_String &filename,
		       const int ngram_order);
		       
static double find_gram_prob(EST_VTPath *p,int *state);

// special stuff for non-sliding window ngrams
static double find_extra_gram_prob(EST_VTPath *p,int *state, int time);
static void get_history(EST_StrVector &history, EST_VTPath *p);
static void fill_window(EST_StrVector &window,EST_StrVector &history,
			EST_VTPath *p,const int time);
static int is_a_special(const EST_String &s, int &val);
static int max_history=0;

static EST_Ngrammar ngram;
static EST_String pstring = SENTENCE_START_MARKER;
static EST_String ppstring = SENTENCE_END_MARKER;
static float lm_scale = 1.0;
static float ob_scale = 1.0;
static float ob_scale2 = 1.0;

// pruning beam widths
static float beam=-1;
static float ob_beam=-1;
static int n_beam = -1;

static bool trace_on = FALSE;

// always logs
static double ob_log_prob_floor = SAFE_LOG_ZERO;  
static double ob_log_prob_floor2 = SAFE_LOG_ZERO;  
static double lm_log_prob_floor = SAFE_LOG_ZERO;  

int btest_debug = FALSE;
static EST_String out_file = "";
static EST_StrList vocab;
static EST_Track observations;  
static EST_Track observations2;  
static EST_TList<EST_StrVector> given; // to do : convert to array for speed
int using_given=FALSE;

// default is that obs are already logs
int take_logs = FALSE;
int num_obs = 1;




/** @name  <command>viterbi</command> <emphasis>Combine n-gram model and likelihoods to estimate posterior probabilities</emphasis>
  * @id viterbi-manual
  * @toc
 */

//@{

/**@name Synopsis
  */
//@{

//@synopsis

/**
viterbi is a simple time-synchronous Viterbi decoder. It finds the
most likely sequence of items drawn from a fixed vocabulary, given
frame-by-frame observation probabilities for each item in that
vocabulary, and a ngram grammar. Possible uses include:

<itemizedlist>
<listitem><para>Simple speech recogniser back end</para></listitem>
</itemizedlist>

viterbi can optionally use two sets of frame-by-frame observation
probabilities in a weighted-sum fashion. Also, the ngram language model
is not restricted to the conventional sliding window type in which the
previous n-1 items are the ngram context. Items in the ngram context
at each frame may be given. In this case, the user must provide a file
containing the ngram context: one (n-1) tuple per line. To include
items from the partial Viterbi path so far (i.e. found at recognition
time, not given) the special notation <-N> is used where N indicates
the distance back to the item required. For example <-1> would
indicate the item on the partial Viterbi path at the last frame. See
\Ref{Examples}.

<formalpara>
<para><title>Pruning</title></para>

<para>
Three types of pruning are available to reduce the size of the search
space and therefore speed up the search:

<itemizedlist>
<listitem><para>Observation pruning</para></listitem>
<listitem><para>Top-N pruning at each frame</para></listitem>
<listitem><para>Fixed width beam pruning</para></listitem>
</itemizedlist>

</para>
</formalpara>

*/

//@}

/**@name Options
  */
//@{

//@options


//@}

int main(int argc, char **argv)
{
    EST_StrList files;
    EST_Option al;
    EST_Relation wstream;
    double floor; // a temporary

    parse_command_line(argc, argv, 
       EST_String("[observations file] -o [output file]\n")+
       "Summary: find the most likely path through a sequence of\n"+
       "         observations, constrained by a language model.\n"+
       "-ngram <string>     Grammar file, required\n"+
       "-given <string>     ngram left contexts, per frame\n"+
       "-vocab <string>     File with names of vocabulary, this\n"+
       "                    must be same number as width of observations, required\n"+
       "-ob_type <string>   Observation type : likelihood .... and change doc\"probs\" or \"logs\" (default is \"logs\")\n"+
       "\nFloor values and scaling (scaling is applied after floor value)\n"+
       "-lm_floor <float>   LM floor probability\n"+
       "-lm_scale <float>   LM scale factor factor (applied to log prob)\n"+
       "-ob_floor <float>   Observations floor probability\n"+
       "-ob_scale <float>   Observation scale factor (applied to prob or log prob, depending on -ob_type)\n\n"+
       "-prev_tag <string>\n"+
       "                 tag before sentence start\n"+
       "-prev_prev_tag <string>\n"+
       "                 all words before 'prev_tag'\n"+
       "-last_tag <string>\n"+
       "                 after sentence end\n"+
       "-default_tags    use default tags of "+SENTENCE_START_MARKER+","
			SENTENCE_END_MARKER+" and "+SENTENCE_END_MARKER+"\n"+
       "                 respectively\n\n"+

       "-observes2  <string> second observations (overlays first, ob_type must be same)\n"+
       "-ob_floor2 <float>  \n"+
       "-ob_scale2 <float>  \n\n"+
       "-ob_prune  <float> observation pruning beam width (log) probability\n"+
       "-n_prune   <int>   top-n pruning of observations\n"+
       "-prune     <float> pruning beam width (log) probability\n"+
       "-trace             show details of search as it proceeds\n",
			files, al);

    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";

    if (files.length() != 1)
      {
	cerr << argv[0];
	cerr << ": you must give exactly one observations file on the command line";
	cerr << endl;
	cerr << "(use -observes2 for optional second observations)" << endl;
	exit(-1);
      }

    if (al.present("-ngram"))
    {
	ngram.load(al.val("-ngram"));
    }
    else
    {
	cerr << argv[0] << ": no ngram specified" << endl;
	exit(-1);
    }

    if(!al.present("-vocab"))
      {
	cerr << "You must provide a vocabulary file !" << endl;
	exit(-1);
      }

    load_wstream(files.first(),al.val("-vocab"),wstream,observations);
    if (al.present("-observes2"))
    {
	load_wstream(al.val("-observes2"),al.val("-vocab"),wstream,observations2);
	num_obs = 2;
    }

    if (al.present("-given"))
    {
	load_given(al.val("-given"),ngram.order());
	using_given=TRUE;
    }

    if (al.present("-lm_scale"))
	lm_scale = al.fval("-lm_scale");
    else
	lm_scale = 1.0;

    if (al.present("-ob_scale"))
	ob_scale = al.fval("-ob_scale");
    else
	ob_scale = 1.0;

    if (al.present("-ob_scale2"))
	ob_scale2 = al.fval("-ob_scale2");
    else
	ob_scale2 = 1.0;

    if (al.present("-prev_tag"))
	pstring = al.val("-prev_tag");
    if (al.present("-prev_prev_tag"))
	ppstring = al.val("-prev_prev_tag");

    // pruning
    if (al.present("-prune"))
	beam = al.fval("-prune");
    else
	beam = -1; // no pruning

    if (al.present("-ob_prune"))
	ob_beam = al.fval("-ob_prune");
    else
	ob_beam = -1; // no pruning

    if (al.present("-n_prune"))
    {
	n_beam = al.ival("-n_prune");
	if(n_beam <= 0)
	{
	    cerr << "WARNING : " << n_beam;
	    cerr << " is not a reasonable value for -n_prune !" << endl;
	}
    }
    else
	n_beam = -1; // no pruning
    


    if (al.present("-trace"))
	trace_on = TRUE;

    // language model floor
    if (al.present("-lm_floor"))
    {
	floor = al.fval("-lm_floor");
	if(floor < 0)
	{
	    cerr << "Error : LM floor probability is negative !" << endl;
	    exit(-1);
	}
	else if(floor > 1)
	{
	    cerr << "Error : LM floor probability > 1 " << endl;
	    exit(-1);
	}
	lm_log_prob_floor = safe_log(floor);
    }

    // observations floor
    if (al.present("-ob_floor"))
    {
	floor = al.fval("-ob_floor");
	if(floor < 0)
	{
	    cerr << "Error : Observation floor probability is negative !" << endl;
	    exit(-1);
	}
	else if(floor > 1)
	{
	    cerr << "Error : Observation floor probability > 1 " << endl;
	    exit(-1);
	}
	ob_log_prob_floor = safe_log(floor);
    }

    if (al.present("-ob_floor2"))
    {
	floor = al.fval("-ob_floor2");
	if(floor < 0)
	{
	    cerr << "Error : Observation2 floor probability is negative !" << endl;
	    exit(-1);
	}
	else if(floor > 1)
	{
	    cerr << "Error : Observation2 floor probability > 1 " << endl;
	    exit(-1);
	}
	ob_log_prob_floor2 = safe_log(floor);
    }
    

    if (al.present("-ob_type"))
    {
	if(al.val("-ob_type") == "logs")
	    take_logs = false;
	else if(al.val("-ob_type") == "probs")
	    take_logs = true;
	else
	{
	    cerr << "\"" << al.val("-ob_type") 
		<< "\" is not a valid ob_type : try \"logs\" or \"probs\"" << endl;
	    exit(-1);
	}
    }

    if(do_search(wstream))
	print_results(wstream);
    else
	cerr << "No path could be found." << endl;

    return 0;
}

static void print_results(EST_Relation &wstream)
{
    EST_Item *s;
    float pscore;
    EST_String predict;
    FILE *fd;

    if (out_file == "-")
	fd = stdout;
    else if ((fd = fopen(out_file,"wb")) == NULL)
    {
	cerr << "can't open \"" << out_file << "\" for output" << endl;
	exit(-1);
    }

    for (s=wstream.head(); s != 0 ; s=s->next())
    {
	predict = s->f("best").string();
	pscore = s->f("best_score");
	fprintf(fd,"%s %f\n",(const char *)predict,pscore);
    }

    if (out_file != "")
	fclose(fd);

}

static bool do_search(EST_Relation &wstream)
{
    // Apply Ngram to matrix of probs 
    int states;

    states = ngram.num_states();
    EST_Viterbi_Decoder vc(vit_candlist,vit_npath,states);

    vc.initialise(&wstream);

    if((beam > 0) || (ob_beam > 0))
	vc.set_pruning_parameters(beam,ob_beam);

    if(trace_on)
    {
	vc.turn_on_trace();
	cerr << "Starting Viterbi search..." << endl;
    }

    vc.search();

    return vc.result("best");  // adds fields to w with best values 

}

static void load_wstream(const EST_String &filename,
			 const EST_String &vfile, 
			 EST_Relation &w,
			 EST_Track &obs)
{
    // Load in vocab and probs into Stream (this isn't general)
    EST_String word, pos;
    int i=-1;

    if(vocab.empty())
	load_vocab(vfile);

    if (obs.load(filename,0.10) != 0)
    {
	cerr << "can't find observations file \"" << filename << "\"" << endl;
	exit(-1);
    }

    if (vocab.length() != obs.num_channels())
    {
	cerr << "Number in vocab (" << vocab.length() << 
	    ") not equal to observation's width (" <<
		obs.num_channels() << ")" << endl;
	exit(-1);
    }
	
    if(w.empty())
	for (i=0; i < obs.num_frames(); i++)
	    add_word(w,itoString(i),i);
}


static void load_given(const EST_String &filename,
		       const int ngram_order)
{

    EST_String word, pos;
    EST_Litem *p;
    int i,j;

    if (load_TList_of_StrVector(given,filename,ngram_order-1) != 0)
    {
	cerr << "can't load given file \"" << filename << "\"" << endl;
	exit(-1);
    }

    // set max history
    for (p = given.head(); p; p = p->next())
    {
	for(i=0;i<given(p).length();i++)
	    if(	is_a_special( given(p)(i), j) && (-j > max_history))
		max_history = -j;
	
    }
    
}

static void load_vocab(const EST_String &vfile)
{
    // Load vocabulary (strings)
    EST_TokenStream ts;

    if (ts.open(vfile) == -1)
    {
	cerr << "can't find vocab file \"" << vfile << "\"" << endl;
	exit(-1);
    }

    while (!ts.eof())
	if (ts.peek() != "")
	    vocab.append(ts.get().string());

    ts.close();
}

static void add_word(EST_Relation &w, const EST_String &word, int pos)
{
    EST_Item *item = w.append();
    
    item->set_name(word);
    item->set("pos",pos);
} 

static EST_VTCandidate *vit_candlist(EST_Item *s,EST_Features &f)
{
    // Return a list of new candidates from this point 
    double prob=1.0,prob2=1.0;
    int i;
    EST_Litem *p;
    int observe;
    EST_VTCandidate *all_c = 0;
    EST_VTCandidate *c;
    (void)f;

    observe = s->f("pos");  // index for observations TRACK
    for (i=0,p=vocab.head(); i < observations.num_channels(); i++,p=p->next())
    {
	c = new EST_VTCandidate;
	c->name = vocab(p);  // to be more efficient this could be the index
	prob = observations.a(observe,i);
	if(num_obs == 2)
	    prob2 = observations2.a(observe,i);

	if(take_logs)
	{
	    prob = safe_log10(prob);
	    if (prob < ob_log_prob_floor)
		prob = ob_log_prob_floor;

	    if(num_obs == 2)
	    {
		prob2 = safe_log10(prob2);
		if (prob2 < ob_log_prob_floor2)
		    prob2 = ob_log_prob_floor2;
	    }
	}
	else // already in logs
	{
	    if (prob < ob_log_prob_floor)
		prob = ob_log_prob_floor;
	    if ((num_obs == 2) && (prob2 < ob_log_prob_floor2))
		prob2 = ob_log_prob_floor2;
	}

	prob *= ob_scale;
	prob2 *= ob_scale2;

	if(num_obs == 2)
	    c->score = prob + prob2;
	else
	    c->score = prob;

	c->next = all_c;
	c->s = s;
	all_c = c;
    }

    if(n_beam > 0)
    {
	// N.B. this might be very time-consuming
	top_n_candidates(all_c);
    }

    return all_c;
}

static EST_VTPath *vit_npath(EST_VTPath *p,EST_VTCandidate *c,
			     EST_Features &f)
{
    // Build a (potential) new path link from this previous path and 
    // This candidate 
    EST_VTPath *np = new EST_VTPath;
    double lprob,prob;
    EST_String prev,ttt;
    (void)f;

    np->c = c;
    np->from = p;

    // are we using extra info ?
    if(using_given)
	// time of candidate is
	// c->s->f("pos");
	prob = find_extra_gram_prob(np,&np->state,c->s->f("pos"));
    else
	prob = find_gram_prob(np,&np->state);

    lprob = safe_log10(prob);
    if (lprob < lm_log_prob_floor)
	lprob =	lm_log_prob_floor;

    lprob *= lm_scale;

    np->f.set("lscore",(c->score+lprob)); // simonk : changed prob to lprob
    if (p==0)
	np->score = (c->score+lprob);
    else
	np->score = (c->score+lprob) + p->score;

    return np;
}

static double find_gram_prob(EST_VTPath *p,int *state)
{
    // Look up transition probability from *state for name.
    // Return probability and update state
    double prob=0.0,nprob;
    int i,f=FALSE;
    EST_VTPath *pp;
    
    EST_StrVector window(ngram.order());
    for (pp=p->from,i=ngram.order()-2; i >= 0; i--)
    {
	if (pp != 0)
	{
	    window[i] = pp->c->name.string();
	    pp = pp->from;
	}
	else if (f)
	    window[i] = ppstring;
	else
	{
	    window[i] = pstring;
	    f = TRUE;
	}
    }
    window[ngram.order()-1] = p->c->name.string();
    const EST_DiscreteProbDistribution &pd = ngram.prob_dist(window);
    if (pd.samples() == 0)
	prob = 0;
    else
	prob = (double)pd.probability(p->c->name.string());
    
    for (i=0; i < ngram.order()-1; i++)
	window[i] = window(i+1);
    ngram.predict(window,&nprob,state);

    return prob;
}


static double find_extra_gram_prob(EST_VTPath *p,int *state,int time)
{

    int i;
    double prob=0.0,nprob;
    EST_StrVector window(ngram.order());
    EST_StrVector history(max_history);

    get_history(history,p);

    fill_window(window,history,p,time);

    /*
    cerr << "Looking up ngram ";
    for(i=0;i<window.num_points();i++)
	cerr << window(i) << " ";
    cerr << endl;
    */

    const EST_DiscreteProbDistribution &pd = ngram.prob_dist(window);
    if (pd.samples() == 0)
	prob = 0;
    else
	prob = (double)pd.probability(p->c->name.string());

    // shift history, adding latest item at 'end' (0)
    if(max_history>0)
    {
	for(i=history.length()-1;i>0;i--)
	    history[i] = history(i-1);
	history[0] = p->c->name.string();
    }

    fill_window(window,history,p,time+1);
    ngram.predict(window,&nprob,state);

    //cerr << endl << endl;

    return prob;

}

static void get_history(EST_StrVector &history, EST_VTPath *p)
{

    EST_VTPath *pp;
    int i,f=FALSE;
    for (pp=p->from,i=0; i < history.length(); i++)
    {
	
	if (pp != 0)
	{
	    history[i] = pp->c->name.string();
	    pp = pp->from;
	}
	else if (f)
	    history[i] = ppstring;
	else
	{
	    history[i] = pstring;
	    f = TRUE;
	}
    }

}

static void fill_window(EST_StrVector &window,EST_StrVector &history,
			EST_VTPath *p,const int time)
{
    // Look up transition probability from *state for name.
    // Return probability and update state
    int i,j;
    EST_String s;

    // can we even do it?
    if( time >= given.length() )
	return;

    // format should be run-time defined, but try this for now
    // first n-1 things in window come from 'given'
    // last one is predictee

    // also want vocab and grammar mismatch allowed !!!!!!

    // predictee
    window[ngram.order()-1] = p->c->name.string();

    // given info for this time
    EST_StrVector *this_g = &(given.nth(time)); // inefficient to count down a list


    for(i=0;i<ngram.order()-1;i++)
    {

	if( is_a_special( (*this_g)(i), j))
	    window[i] = history(-1-j); // j=-1 -> (0)   j=-2 -> (1)   etc.
	else
	    window[i] = (*this_g)(i);
    }
}



static int is_a_special(const EST_String &s, int &val)
{

    // special is "<int>"

    EST_String tmp;
    if(s.contains("<") && s.contains(">"))
    {
	tmp = s.after("<");
	tmp = tmp.before(">");
	val = atoi(tmp);
	//cerr << "special " << tmp << "=" << val << endl;
	return TRUE;
    }
    return FALSE;
}

static void top_n_candidates(EST_VTCandidate* &all_c)
{
    // keep the n most likely candidates
    // avoiding a full sort of the (potentially long) list

    EST_VTCandidate *top_c=NULL,*p,*q,*this_best, *prev_to_best;
    int i;
    if(n_beam < 1)
	return; // do nothing

    // here we assume big is always good
    //if(!big_is_good)
    //score_multiplier = -1;

    for(i=0;i<n_beam;i++)
    {

	// head of the list is best guess
	this_best=all_c;
	prev_to_best=NULL;

	// find best candidate in all_c
	q=NULL;;
	for(p=all_c;p!= NULL;q=p,p=p->next)
	{
	    //cerr << "item : " << p->score << endl;
	    if(p->score > this_best->score)
	    {
		this_best = p;
		prev_to_best=q;
	    }
	}
	
	if(this_best == NULL)
	    break; // give up now - must have run out of candidates

	// move best candidate over to new list
	if(prev_to_best == NULL)
	    // best was head of list
	    all_c = this_best->next;
	else
	    // best was not head of all_c
	    prev_to_best->next = this_best->next;

	    this_best->next = top_c;
	    top_c = this_best;
    }

    delete all_c;
    all_c = top_c;

/*
    cerr << "Here are the top " << n_beam << " candidates" << endl;
    for(p=all_c;p != NULL;p=p->next)
	cerr << p->score << endl;
*/
}


/**@name Examples

Example 'given' file (items f and g are in the vocabulary), the ngram
is a 4-gram.

<para>
<screen>
<-2> g g
<-1> g f
<-1> f g
<-2> g g
<-3> g g
<-1> g f
</screen>
</para>

*/
//@{
//@}



//@}
