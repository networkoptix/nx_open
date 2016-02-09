/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                       Copyright (c) 1996,1997                         */
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
/*                 Authors:  Alan W Black                                */
/*                 Date   :  July 1996                                   */
/*-----------------------------------------------------------------------*/
/*  A viterbi decoder                                                    */
/*                                                                       */
/*  User provides the candidates, target and combine score function      */
/*  and it searches for the best path through the candidates             */
/*                                                                       */
/*=======================================================================*/
#include <cstdio>
#include "EST_viterbi.h"

static void init_paths_array(EST_VTPoint *n,int num_states);
static void debug_output_1(EST_VTPoint *p,EST_VTCandidate *c, 
			   EST_VTPath *np, int i);

EST_VTPoint::~EST_VTPoint()
{
    int i;

    if (paths != 0) delete paths;
    if (num_states != 0)
    {
	for (i=0; i<num_states; i++)
	    if (st_paths[i] != 0)
		delete st_paths[i];
	delete [] st_paths;
    }
    if (cands != 0) delete cands;
    if (next != 0) delete next;
}

EST_Viterbi_Decoder::EST_Viterbi_Decoder(uclist_f_t a, unpath_f_t b)
  : vit_a_big_number(1.0e10)
{
    beam_width = 0;
    cand_width = 0;
    user_clist = a;
    user_npath = b;
    num_states = 0;

    do_pruning = FALSE;
    overall_path_pruning_envelope_width = -1;
    candidate_pruning_envelope_width = -1;

    debug = FALSE;
    trace = FALSE;
    big_is_good = TRUE;  // for probabilities it is
}

EST_Viterbi_Decoder::EST_Viterbi_Decoder(uclist_f_t a, unpath_f_t b, int s)
  : vit_a_big_number(1.0e10)
{
    beam_width = 0;
    cand_width = 0;
    user_clist = a;
    user_npath = b;

    do_pruning = FALSE;
    overall_path_pruning_envelope_width = -1;
    candidate_pruning_envelope_width = -1;

    // if s == -1 number of states is determined by number of cands
    // this is specific to a particular search but very efficient
    num_states = s;  // can do the lattice search more directly 
    debug = FALSE;
    trace = FALSE;
    big_is_good = TRUE;  // for probabilities it is
}

EST_Viterbi_Decoder::~EST_Viterbi_Decoder()
{
    delete timeline;
}

void EST_Viterbi_Decoder::initialise(EST_Relation *p)
{
    // Creates a time line with each point pointing at each item in p
    EST_Item *i;
    EST_VTPoint *t = 0,*n;

    for (i=p->head(); i != 0; i=i->next())
    {
	n = new EST_VTPoint;
	n->s = i;
	if (num_states > 0)  
	    init_paths_array(n,num_states);
	if (t == 0)
	    timeline = n;
	else
	    t->next = n;
	t=n;
    }
    // Extra one at the end for final paths
    n = new EST_VTPoint;
    if (num_states > 0)
	init_paths_array(n,num_states);

    // Need init path on first point so a search can start
    if (num_states == 0)   // general search case 
	timeline->paths = new EST_VTPath;
    if (num_states == -1)
	init_paths_array(timeline,1);  // a start point

    if (t == 0)
	timeline = n;   // its an empty stream
    else
	t->next = n;
}

static void init_paths_array(EST_VTPoint *n,int num_states)
{
    // Create the states array and initialize it
    int j;
    
    n->num_states = num_states;
    n->st_paths = new EST_VTPath*[num_states];
    for (j=0;j<num_states;j++)
	n->st_paths[j] = 0;
}

const int EST_Viterbi_Decoder::betterthan(const float a,const float b) const
{
    // Some thing big is better, others want it to be as small as possible
    // this just tells you if a is better than b by checking the variable
    // in the decoder itself.

    // For probabilities big_is_good == TRUE (or log probabilities)
    
    if (big_is_good)
	return (a > b);
    else
	return (a < b);
}

static int init_dynamic_states(EST_VTPoint *p, EST_VTCandidate *cands)
{
    // In a special (hmm maybe not so special), the number of "states"
    // is the number of candidates
    EST_VTCandidate *c;
    int i;

    for (i=0, c=cands; c != 0; c=c->next,i++)
	c->pos = i;
    init_paths_array(p,i);
    
    return i;
}

void EST_Viterbi_Decoder::set_pruning_parameters(float beam, float
						 ob_beam)
{

    do_pruning = TRUE;
    overall_path_pruning_envelope_width = beam;
    candidate_pruning_envelope_width = ob_beam;

}

void EST_Viterbi_Decoder::turn_on_debug()
{
    debug = TRUE;
}

void EST_Viterbi_Decoder::turn_on_trace()
{
    trace = TRUE;
}


void EST_Viterbi_Decoder::search(void)
{
    // Searches for the best path 
    EST_VTPoint *p;
    EST_VTPath *t,*np;
    EST_VTCandidate *c;
    int i=0;

    double best_score=0.0,score_cutoff=0.0;
    double best_candidate_score=0.0,candidate_cutoff=0;
    int dcount,pcount;
    int cand_count=0, cands_considered=0;

    for (p=timeline; p->next != 0; p=p->next)
    {   // For each point in time 
	// Find the candidates
	p->cands  = (*user_clist)(p->s,f);  // P(S|B)
	if (do_pruning)
	    prune_initialize(p,best_score,best_candidate_score,
			     score_cutoff,candidate_cutoff,
			     cand_count);
	if (num_states != 0)  // true viterbi -- optimized for states
	{
	    if (num_states == -1)  // special case, dynamic state size
		init_dynamic_states(p->next,p->cands);

	    cands_considered=0; // moved by simonk
	    for (i=0; i<p->num_states; i++)
	    {   // for each path up to here
		//cands_considered=0;
		if (((p == timeline) && i==0) || (p->st_paths[i] != 0))
		    for (c=p->cands; c != 0; c=c->next)
		    {   
			// for each new candidate
			// candidate pruning (a.k.a. observation pruning)
			if(!do_pruning || 
			   betterthan(c->score,candidate_cutoff))
			{
			    cands_considered++;
			    // Find path extension costs
			    np = (*user_npath)(p->st_paths[i],c,f);
			    if (debug) debug_output_1(p,c,np,i);
			    if (do_pruning && betterthan(np->score,best_score))
			    {
				best_score = np->score;
				if(big_is_good)
				    score_cutoff = best_score 
					- overall_path_pruning_envelope_width;
				else
				    score_cutoff = best_score 
					+ overall_path_pruning_envelope_width;
			    }
			    // can prune here, although score_cutoff will 
			    // be generally too generous at this point.
			    // It's unclear whether this pruning takes more
			    // time than it saves !
			    if(!do_pruning||betterthan(np->score,score_cutoff))
				vit_add_paths(p->next,np);
			    else
				delete np;
			}
		    }
	    }

	    if (do_pruning)
	    {
		if(big_is_good)
		    score_cutoff = 
			best_score - overall_path_pruning_envelope_width;
		else
		    score_cutoff = 
			best_score + overall_path_pruning_envelope_width;
		if(trace)
		{
		    cerr << "Considered " << cands_considered << " of ";
		    cerr << cand_count*p->num_states << " candidate paths" << endl;
		    cerr << "FRAME: best score " << best_score;
		    cerr << "  score cutoff " << score_cutoff << endl;
		    cerr << "       best candidate score " << best_candidate_score;
		    cerr << "  candidate cutoff " << candidate_cutoff << endl;
		}

		dcount=0; pcount=0;
		for (i=0; i<p->next->num_states; i++)
		    if(p->next->st_paths[i] != 0)
		    {
			pcount++;
			if(!betterthan(p->next->st_paths[i]->score,
				       score_cutoff))
			{
			    delete p->next->st_paths[i];
			    p->next->st_paths[i] = 0;
			    dcount++;
			}
		    }
		if(trace)
		{
		    cerr << "Pruned " << dcount << " of " << pcount;
		    cerr << " paths" << endl << endl;
		}
	    }
	}
	else                  // general beam search
	    for (t=p->paths; t != 0; t=t->next)
	    {   // for each path up to here
		for (c=p->cands; c != 0; c=c->next)
		{   // for each new candidate
		    np = (*user_npath)(t,c,f);
		    add_path(p->next,np);      // be a little cleverer
		}
	    }
	if (debug) fprintf(stdout,"\n");
    }

}

void EST_Viterbi_Decoder::
     prune_initialize(EST_VTPoint *p,
		      double &best_score, double &best_candidate_score,
		      double &score_cutoff, double &candidate_cutoff,
		      int &cand_count)
{   // Find best candidate, count them and set some vars.
    EST_VTCandidate *c;
    if (big_is_good)
    {
	best_score = -vit_a_big_number;
	best_candidate_score = -vit_a_big_number;
	score_cutoff = -vit_a_big_number;
	candidate_cutoff = - candidate_pruning_envelope_width;
    }
    else
    {
	best_candidate_score = vit_a_big_number;
	best_score = vit_a_big_number;
	score_cutoff = vit_a_big_number;
	candidate_cutoff = candidate_pruning_envelope_width;
    }

    for (cand_count=0,c=p->cands; c; c=c->next,cand_count++)
	if (betterthan(c->score,best_candidate_score))
	    best_candidate_score = c->score;
    candidate_cutoff += best_candidate_score;
}

static void debug_output_1(EST_VTPoint *p,EST_VTCandidate *c, 
			   EST_VTPath *np,int i)
{
    printf("%s: ",(const char *)p->s->name());
    cout << c->name;
    printf(" %1.3f B %1.3f (%1.3f) st %d s %1.3f ",
	   np->c->score,
	   (np->c->score==0 ? 0 : 
	    ((float)np->f("lscore"))/np->c->score),
	   (float)np->f("lscore"),np->state,
	   np->score);
    if (p->st_paths[i] == 0)
	cout << "(I)" << endl;
    else
	cout << p->st_paths[i]->c->name << endl;
}

void EST_Viterbi_Decoder::vit_add_paths(EST_VTPoint *pt, EST_VTPath *np)
{
    // Add set of paths 
    EST_VTPath *p,*next_p;

    for (p=np; p != 0; p=next_p)
    {
	next_p = p->next;  // need to save this as p could be deleted
	vit_add_path(pt,p);
    }

}
void EST_Viterbi_Decoder::vit_add_path(EST_VTPoint *p, EST_VTPath *np)
{
    // We are doing true viterbi so we need only keep the best
    // path for each state.  This means we can index into the
    // array of paths ending at P and only keep np if its score
    // is better than any other path of that state

    if ((np->state < 0) || (np->state > p->num_states))
    {
	cerr << "EST_Viterbi: state too big (" << np->state << ")" << endl;
    }
    else if ((p->st_paths[np->state] == 0) ||
	     (betterthan(np->score,p->st_paths[np->state]->score)))
    {
	// This new one is better so replace it
	if (p->st_paths[np->state] != 0)
	    delete p->st_paths[np->state];
	p->st_paths[np->state] = np;
    }
    else
	delete np;
}

bool EST_Viterbi_Decoder::vit_prune_path(double path_score, double score_cutoff)
{

    // a bit of a simple function !!

    // if it falls below cutoff, then prune point
    // typically will only be one path at this point anyway (Viterbi)
    if(!betterthan(path_score,score_cutoff))
	return TRUE;

    return FALSE;
}



void EST_Viterbi_Decoder::add_path(EST_VTPoint *p, EST_VTPath *np)
{
    // Add new path to point,  Prunes as appropriate to strategy
    EST_VTPath *pp;

    if (p == 0)
	cerr << "Viterbi: tried to add path to NULL point\n";
    else 
    {
	if ((beam_width == 0) ||            // Add if no beam restrictions or
	    (p->num_paths < beam_width) ||  //        beam not filled  or
	    (betterthan(np->score,p->paths->score)))//this is better than best
//	    (np->score > p->paths->score))  //        this is better than best
	{
	    EST_VTPath **l = &p->paths;
	    EST_VTPath *a;
	    
	    for (a=p->paths; /* scary */ ; a=a->next)
	    {
		if ((a == 0) || (betterthan(a->score,np->score)))
		{   // Add new path here 
		    np->next = a;
		    *l = np;
		    p->num_paths++;
		    break;
		}
		l = &a->next;
	    }
	    // Prune the first one of the list 
	    if ((beam_width > 0) &&
		(p->num_paths > beam_width))
	    {
		pp = p->paths;
		p->paths = pp->next;
		pp->next = 0;
		p->num_paths--;
		delete pp;
	    }
	}
	else
	{
	    delete np;  // failed to make it
	}
    }

}

EST_VTCandidate *EST_Viterbi_Decoder::add_cand_prune(EST_VTCandidate *newcand,
					     EST_VTCandidate *allcands)
{
    // Add newcand to allcand, in score order and prune it to 
    // cand_width, delete newcand if its not good enough
    EST_VTCandidate *newlist = allcands;
    EST_VTCandidate *pp;
    int numcands;
    
    if (allcands == 0)
	numcands = 0;
    else
	numcands = allcands->pos;
    
    if ((cand_width == 0) ||	// Add if no candbeam restrictions or
	(numcands < cand_width) || //        candbeam not filled  or
	(betterthan(newcand->score,allcands->score))) //this better than best
    {
	EST_VTCandidate **l = &newlist;
	EST_VTCandidate *a;
	
	for (a=newlist;		/* scary */ ; a=a->next)
	{
	    if ((a == 0) || (betterthan(a->score,newcand->score)))
	    {			// Add new path here 
		newcand->next = a;
		*l = newcand;
		numcands++;
		break;
	    }
	    l = &a->next;
	}
	// Prune the first one off the list 
	if ((cand_width > 0) &&
	    (numcands > cand_width))
	{
	    pp = newlist;
	    newlist = pp->next;
	    pp->next = 0;
	    numcands--;
	    delete pp;
	}
    }
    else
	delete newcand;		// failed to make it
    
    newlist->pos = numcands;
    return newlist;
}

bool EST_Viterbi_Decoder::result(const EST_String &n)
{
    // Finds best path through the search space (previously searched)
    // Adds field to the EST_Items, named n, with chosen value 
    EST_VTPath *p;

    if ((timeline == 0) || (timeline->next == 0))
	return TRUE;  // it's an empty list so it has succeeded
    p = find_best_end();
    if (p == 0)
	return FALSE; // there isn't any answer

    for (; p != 0; p=p->from)
    {
	// Hmm need the original EST_Item
	if (p->c != 0)
	{
	    p->c->s->set_val(n,p->c->name);
	    p->c->s->set(n+"_score",p->f.F("lscore",0.0));
	}
    }
    return TRUE;
}

bool EST_Viterbi_Decoder::result( EST_VTPath **bestPathEnd )
{
  // Finds best path through the search space (previously searched)
  *bestPathEnd = 0; 
  
  if ((timeline == 0) || (timeline->next == 0))
    return TRUE;  // it's an empty list so it has succeeded

  *bestPathEnd = find_best_end();

  if (*bestPathEnd == 0)
    return FALSE; // there isn't any answer

  return TRUE;
}


void EST_Viterbi_Decoder::copy_feature(const EST_String &n)
{
    // Copy feature from path to related stream
    EST_VTPath *p;

    p = find_best_end();
    if(p == 0)
	return;

    for (; p != 0; p=p->from)
    {
	// Hmm need the original EST_Item
	if ((p->c != 0) && (p->f.present(n)))
	    p->c->s->set_val(n,p->f(n));
    }
}

EST_VTPath *EST_Viterbi_Decoder::find_best_end() const
{
    EST_VTPoint *t;
    double best,worst;
    EST_VTPath *p, *best_p=0;
    int i;
    // I'd like to use HUGE_VAL or FLT_MAX for this but its not portable 
    // (on alphas)
    
    if (big_is_good) 
	worst = -vit_a_big_number;	// worst possible;
    else
	worst = vit_a_big_number;	// worst possible;
    best = worst;		// hopefully we can find something better;
    
    for (i=0,t=timeline; t->next != 0; t=t->next,i++)
    {
	if ((t->num_states == 0) && (t->num_paths == 0))
	{
	    cerr << "No paths at frame " << i << " " << t->s->name() << endl;
	    return 0;
	}
    }

    if (num_states != 0)
	for (i=0; i<t->num_states; i++)
	{
	    if ((t->st_paths[i] != 0) &&
		(betterthan(t->st_paths[i]->score,best)))
	    {
		best = t->st_paths[i]->score;
		best_p = t->st_paths[i];
	    }
	}
    else
	for (p=t->paths; p!=0; p=p->next)
	{
	    if (betterthan(p->score,best))
	    {
		best = p->score;
		best_p = p;
	    }
	}


    if (debug)
    {
	if (best == worst)
	    cerr << "Failed to find path" << endl;
	cout << "Best score is " << best << endl;
    }

    return best_p;
}

