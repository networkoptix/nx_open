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
/*             Author :  Alan W Black                                    */
/*             Date   :  June 1997                                       */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* A SCFG chart parser                                                   */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include "siod.h"
#include "EST_math.h"
#include "EST_SCFG.h"
#include "EST_SCFG_Chart.h"

EST_SCFG_Chart_Edge::EST_SCFG_Chart_Edge()
{
}

EST_SCFG_Chart_Edge::EST_SCFG_Chart_Edge(double prob,
					 int d1, int d2,
					 int pos)
{
    p_d1 = d1;
    p_d2 = d2;
    p_pos = pos;
    p_prob = prob;
}

EST_SCFG_Chart_Edge::~EST_SCFG_Chart_Edge()
{
}

LISP EST_SCFG_Chart::print_edge(int start, int end, int p,
				EST_SCFG_Chart_Edge *e)
{
    // Return a lisp representation of the edge

    if (e->prob() == 0)
    {
	return NIL;  // failed
    }
    else if (start+1 == end)
    {
	// unary rule, preterminal
	LISP r = cons(rintern(grammar->nonterminal(p)),
		 cons(flocons(e->prob()),
 	          cons(flocons(start),
		   cons(flocons(end),
		    cons(rintern(grammar->terminal(e->d1())),
			 NIL)))));
	return r;
    }
    else 
    {
	//name prob start end daughters
	EST_SCFG_Chart_Edge *d1, *d2;

	d1 = edges[start][e->pos()][e->d1()];
	d2 = edges[e->pos()][end][e->d2()];

	LISP daughters = 
	    cons(print_edge(start,e->pos(),e->d1(),d1),
		 cons(print_edge(e->pos(),end,e->d2(),d2),
		      NIL));
	LISP r = cons(rintern(grammar->nonterminal(p)),
		 cons(flocons(e->prob()),
		      cons(flocons(start),
			   cons(flocons(end),
				daughters))));
	return r;
    }
}

EST_SCFG_Chart::EST_SCFG_Chart()
{
    n_vertices = 0;
    edges = 0;
    wfst = 0;
    emptyedge = 0;
    grammar_local = TRUE;
    grammar = new EST_SCFG;
}

EST_SCFG_Chart::~EST_SCFG_Chart()
{
    // delete all the vertices
    
    delete_edge_table();
    if (grammar_local)
	delete grammar;
}

void EST_SCFG_Chart::set_grammar_rules(EST_SCFG &imported_grammar)
{
    if (grammar_local)
	delete grammar;
    grammar_local = FALSE;
    grammar = &imported_grammar;
}

void EST_SCFG_Chart::set_grammar_rules(LISP r)
{ 
    grammar->set_rules(r); 
}

void EST_SCFG_Chart::setup_wfst(EST_Relation *s,const EST_String &name)
{
    // Set up well formed substring table from feature name in each
    // stream item in s
    setup_wfst(s->head(),0,name);
}

void EST_SCFG_Chart::setup_wfst(EST_Item *s, EST_Item *e,const EST_String &name)
{
    // Set up well formed substring table from feature name in each
    // stream item in s
    EST_Item *p;
    int n;

    delete_edge_table();
    for (n_vertices=1,p=s; p != e; p=p->next())
	n_vertices++;
    setup_edge_table();

    for (n=0,p=s; p != e; p=p->next(),n++)
    {
	int term = grammar->terminal(p->f(name).string());
	if (term == -1)
	{
	    cerr << "SCFG_Chart: unknown terminal symbol \"" <<
		p->f(name).string() << "\"" << endl;
	    term = 0;  // to avoid crashing
	}
	wfst[n] = new EST_SCFG_Chart_Edge(1.0,term,0,-1);
    }
}

void EST_SCFG_Chart::delete_edge_table()
{
    int i,j,k;

    if (wfst == 0) return;

    for (i=0; i < n_vertices; i++)
    {
	delete wfst[i];
	for (j=0; j < n_vertices; j++)
	{
	    for (k=0; k < grammar->num_nonterminals(); k++)
		if (edges[i][j][k] != emptyedge)
		    delete edges[i][j][k];
	    delete [] edges[i][j];
	}
	delete [] edges[i];
    }
    delete [] wfst;
    delete [] edges;
    delete emptyedge;

    wfst = 0;
    edges = 0;

}

void EST_SCFG_Chart::setup_edge_table()
{
    int i,j,k;
    int nt = grammar->num_nonterminals();

    wfst = new EST_SCFG_Chart_Edge*[n_vertices];
    edges = new EST_SCFG_Chart_Edge ***[n_vertices];
    emptyedge = new EST_SCFG_Chart_Edge(0,0,0,0);

    for (i=0; i < n_vertices; i++)
    {
	wfst[i] = 0;
	edges[i] = new EST_SCFG_Chart_Edge **[n_vertices];
	for (j=0; j < n_vertices; j++)
	{
	    edges[i][j] = new EST_SCFG_Chart_Edge *[nt];
	    for (k=0; k < nt; k++)
		edges[i][j][k] = 0;
	}
    }
}

double EST_SCFG_Chart::find_best_tree_cal(int start,int end,int p)
{
    // Find the best parse for non/terminal p between start and end
    int best_j = -1;
    int best_q = -1, best_r = -1;
    double best_prob = 0;

    if (end-1 == start)
    {
	best_prob = grammar->prob_U(p,wfst[start]->d1());
	if (best_prob > 0)
	    edges[start][end][p] = 
		new EST_SCFG_Chart_Edge(best_prob*wfst[start]->prob(),
					wfst[start]->d1(),0,-1);
	else
	    edges[start][end][p] = emptyedge;
	return best_prob;
    }
    else
    {
	// for all rules expanding p find total and best prob
	double s=0,t_prob,left,right;
	int j,q,r;
	int nt = grammar->num_nonterminals();

	for (q=0; q < nt; q++)
	    for (r=0; r < nt; r++)
	    {
		double pBpqr = grammar->prob_B(p,q,r);
		if (pBpqr > 0)
		{
		    for (j=start+1; j < end; j++)
		    {
			left=find_best_tree(start,j,q);
			if (left > 0)
			{
			    right = find_best_tree(j,end,r);
			    t_prob =  pBpqr * left * right;
			    if (t_prob > best_prob)
			    {
				best_prob = t_prob;
				best_q = q;
				best_r = r;
				best_j = j;
			    }
			    s += t_prob;
			}
		    }
		}
	    }

	if (best_prob > 0)
	    edges[start][end][p] = 
		new EST_SCFG_Chart_Edge(s,best_q,best_r,best_j);
	else
	    edges[start][end][p] = emptyedge;
	return s;
    }
}

void EST_SCFG_Chart::parse(void)
{
    // do the parsing, find best parse only
    if (n_vertices - 1 > 0)
        find_best_tree(0,n_vertices-1,grammar->distinguished_symbol());

}

LISP EST_SCFG_Chart::find_parse()
{
    // Extract the parse from the edge table
    EST_SCFG_Chart_Edge *top;

    top = edges[0][n_vertices-1][grammar->distinguished_symbol()];

    if (top == 0)
	return NIL;   // no parse
    else
	return print_edge(0,n_vertices-1,grammar->distinguished_symbol(),top);
}

void EST_SCFG_Chart::extract_parse(EST_Relation *syn,
				   EST_Relation *word,
				   int force)
{
    // Build a tree stream in Syn linking the leafs of Syn to those
    // in word, force guarantees a parse is necessary (though probably
    // a random one)

    extract_parse(syn,word->head(),0,force);
}

void EST_SCFG_Chart::extract_parse(EST_Relation *syn,
				   EST_Item *s, EST_Item *e, int force)
{
    // Build a tree stream in Syn linking the leafs of Syn to those
    // in word
    EST_Item *p;
    int num_words;

    for (num_words=0,p=s; p != e; p=p->next())
	num_words++;

    if (num_words != (n_vertices-1))
    {
	cerr << "SCFG_Chart: extract_parse, number of items in link stream " <<
	    " different from those in parse tree" << endl;
	return;
    }

    EST_SCFG_Chart_Edge *top;
    EST_Item *w = s;

    top = edges[0][n_vertices-1][grammar->distinguished_symbol()];

    if (top == 0)
	return;   // failed to parse so no parse tree
    else
    {
	EST_Item *ss = syn->append();
	
	extract_edge(0,n_vertices-1,grammar->distinguished_symbol(),top,
		     ss,&w);

	if ((force) && (!daughter1(ss)))  // no parse found but *need* one
	    extract_forced_parse(0,n_vertices-1,ss,w);
	return;
    }
}

void EST_SCFG_Chart::extract_forced_parse(int start, int end,
					  EST_Item *s, EST_Item *w)
{
    // Extract a parse even though one wasn't found (often happens
    // with single word or dual word sentences.
    
    if (start+1 == end)
    {
	s->append_daughter(w);
	s->set_name(grammar->nonterminal(grammar->distinguished_symbol()));
	s->set("prob",0.0);  // maybe should be epsilon
    }
    else
    {
	extract_forced_parse(start,start+1,s->append_daughter(),w);
	EST_Item *st = s->append_daughter();
	st->set_name(grammar->nonterminal(grammar->distinguished_symbol()));
	st->set("prob",0.0);  // maybe should be epsilon
	EST_Item *nw = w->next();
	extract_forced_parse(start+1,end,st,nw);
    }
}

void EST_SCFG_Chart::extract_edge(int start, int end, int p,
				  EST_SCFG_Chart_Edge *e,
				  EST_Item *s,
				  EST_Item **word)
{
    // Build the node for this edge, and all of its daughters

    if (e->prob() == 0)
    {
	return;  // failed
    }
    else if (start+1 == end)
    {
	// unary rule, preterminal
	s->append_daughter((*word));
	s->set_name(grammar->nonterminal(p));	
	s->set("prob",(float)e->prob());
	*word = (*word)->next();  // increment along "word" stream
	return;
    }
    else 
    {
	//name prob start end daughters
	EST_SCFG_Chart_Edge *d1, *d2;

	d1 = edges[start][e->pos()][e->d1()];
	d2 = edges[e->pos()][end][e->d2()];

	// Inserts the new nodes in the tree (and creates new si nodes)
	s->append_daughter();
	s->append_daughter();

	extract_edge(start,e->pos(),e->d1(),d1,daughter1(s),word);
	extract_edge(e->pos(),end,e->d2(),d2,daughter2(s),word);

	s->set_name(grammar->nonterminal(p));	
	s->set("prob",(float)e->prob());

	return;
    }
}

void EST_SCFG_chart_load_relation(EST_Relation &s,LISP sent)
{
    // Set up well formed substring table form lisp list
    // Setup a relation and call the standard method of set up
    LISP w,f;

    for (w=sent; w != NIL; w=cdr(w))
    {
	EST_Item *word = s.append();
	
	if (consp(car(w)))
	{   // a word with other feature info
	    word->set_name(get_c_string(car(car(w))));
	    if (consp(car(cdr(car(w)))))
		for (f=car(cdr(car(w))); f != NIL; f=cdr(f))
		{
		    if (FLONUMP(car(cdr(car(f)))))
			word->set(get_c_string(car(car(f))),
				  get_c_float(car(cdr(car(f)))));
		    else 
			word->set(get_c_string(car(car(f))),
				  get_c_string(car(cdr(car(f)))));
		}
	    else // we assume its a POS value, cause they didn't say
		word->set("name",get_c_string(car(cdr(car(w)))));
	}
	else // for easy we set the pos field to the be the name
	    word->set("name",get_c_string(car(w)));
    }
}

void scfg_parse(EST_Relation *Word, const EST_String &name, 
		EST_Relation *Syntax, EST_SCFG &grammar)
{
    // Parse feature name in Word to build Syntax relation
    // The relations names above are *not* the names of the relations
    // just named to reflect there conceptual usage
    EST_SCFG_Chart chart;

    chart.set_grammar_rules(grammar);
    chart.setup_wfst(Word,name);
    chart.parse();
    chart.extract_parse(Syntax,Word,TRUE);

    return;
}

LISP scfg_parse(LISP string, LISP grammar)
{
    // Parse and return full parse
    EST_SCFG_Chart chart;
    EST_Relation words;
    LISP parse;

    chart.set_grammar_rules(grammar);

    EST_SCFG_chart_load_relation(words,string);
    chart.setup_wfst(&words,"name");
    chart.parse();
    parse = chart.find_parse();

    return parse;
}

LISP scfg_parse(LISP string, EST_SCFG &grammar)
{
    // Parse and return full parse
    EST_SCFG_Chart chart;
    EST_Relation words;
    LISP parse;

    chart.set_grammar_rules(grammar);

    EST_SCFG_chart_load_relation(words,string);
    chart.setup_wfst(&words,"name");
    chart.parse();
    parse = chart.find_parse();

    return parse;
}

LISP scfg_bracketing_only(LISP parse)
{
    if (consp(siod_nth(4,parse)))
    {
	LISP d,ds;

	for (d=cdr(cdr(cdr(cdr(parse)))),ds=NIL; d != NIL; d=cdr(d))
	    ds = cons(scfg_bracketing_only(car(d)),ds);
	return reverse(ds);
    }
    else
	return siod_nth(4,parse);

}

void EST_SCFG_traintest::test_crossbrackets()
{
    // Compare bracketing of best parse to bracketing on original
    // For each sentence parse it (unbracketed) and then
    // find the percentage of valid brackets in parsed version that
    // are valid in the original one.
    int c;
    LISP parse;
    EST_SuffStats cb;
    int failed = 0;
    int fully_contained=0;

    for (c=0; c < corpus.length(); c++)
    {
	LISP flat = siod_flatten(corpus.a_no_check(c).string());
	
	parse =  scfg_parse(flat,*this);
	if (parse == NIL)
	{
	    failed++;
	    continue;
	}

	EST_bracketed_string parsed(scfg_bracketing_only(parse));
	EST_SuffStats vs;

	count_bracket_crossing(corpus.a_no_check(c),parsed,vs);

	if (vs.mean() == 1)
	    fully_contained++;
	cb += vs.mean();
    }

    cout << "cross bracketing " << cb.mean()*100 << " (" << failed << 
	" failed " << (float)(100.0*fully_contained)/corpus.length() <<
	    "% fully consistent from " << corpus.length() 
		<< " sentences)" << endl;

}

void count_bracket_crossing(const EST_bracketed_string &ref,
			    const EST_bracketed_string &test,
			    EST_SuffStats &vs)
{
    int i,j;

    if (ref.length() != test.length())
    {
	EST_error("bracket_crossing: sentences of different lengths");
    }

    for (i=0; i < ref.length(); i++)
	for (j=i+1; j <= ref.length(); j++)
	    if (test.valid(i,j) == 1)
	    {
		if (ref.valid(i,j) == 0)
		    vs += 0;
		else
		    vs += 1;
	    }
}
