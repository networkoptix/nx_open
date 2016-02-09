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
/*                 Author:   Simon King                                  */
/*                 Date   :  May 1998                                    */
/*-----------------------------------------------------------------------*/
/* Simple dynamic programming                                            */
/* e.g. for aligning pronunciations                                      */
/*=======================================================================*/

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include "EST.h"

EST_read_status load_TList_of_StrVector(EST_TList<EST_StrVector> &w,
					const EST_String &filename,
					const int vec_len);

typedef
float (*local_cost_function)(const EST_Item *item1,
			     const EST_Item *item2);

typedef
bool (*local_pruning_function)(const int i,
			       const int j,
			       const int max_i, 
			       const int max_j);

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
	      EST_Item *null_sym);

float local_cost(const EST_Item *s1, const EST_Item *s2);
bool local_prune(const int i, const int j,
		 const int max_i,const int max_j);
static void load_vocab(const EST_String &vfile);
static EST_Item *null_sym;
//static EST_String deleted_marker = "<del>";
//static EST_String inserted_marker = "<ins>";
static bool show_cost=FALSE;
static int prune_width = 100;
//static float path_cost;
int StrVector_index(const EST_StrVector &v, const EST_String &s);

EST_StrList pattern1_l, pattern2_l, vocab_l;
EST_StrVector pattern1, pattern2, vocab;

EST_FMatrix DP_substitution_cost;
EST_FVector DP_deletion_cost;
EST_FVector DP_insertion_cost;
EST_IMatrix DP_path_i,DP_path_j;
EST_FMatrix cost_matrix;

// this are local distance measures, 
// NOT the 1/2/1 weights in the DP recursion formula !
float insertion_cost = 1;
float deletion_cost = 1;
float substitution_cost = 1;


// two possibilities : 
// 1. simple insertion/deletion/substitution penalties
// 2. matrix of distances between all pairs of vocab items, 
//    where vocab includes some null symbol for insertions/deletions

EST_String distance_measure = "simple"; // could be "matrix"




/** @name <command>dp</command> <emphasis> Perform dynamic programming on label sequences</emphasis>
  * @id dp-manual
  * @toc
 */

//@{

/**@name Synopsis
  */
//@{

//@synopsis

/**
dp provides simple dynamic programming to find the lowest cost
alignment of two symbol sequences. Possible uses include:

<itemizedlist>
<listitem><para>Label alignment (e.g. speech recogniser output scoring) </para></listitem>
</itemizedlist>

The costs of inserting/deleting/substituting symbols can be given
either with command line options, or as a file containing a full
matrix of costs. In the former case, all insertions will have the same
cost. In the latter case, each row of the file contains the cost of
replacing a symbol in sequence 1 with each possible symbol in the
vocabulary to align with sequence 2, including a special "place
holder" symbol used for insertions/deletions. See the examples
below. The place holder can be redefined.

The output is an EST utterance with three Relations: the first two are
the input sequences, and the third shows the alignment found by
dynamic programming.

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
    EST_String out_file;
    //int i;
    EST_Relation *path1, *path2, *match;

    null_sym = new EST_Item;
    null_sym->set_name("<null>");

    parse_command_line(argc, argv, 
       EST_String("Usage:\n")+
       "dp  <options> \"pattern 1\" \"pattern 2\"\n"+
       "Find the best alignment of a pair of symbol sequences (e.g. word pronuciations).\n"+
       "-vocab <string>         file containing vocabulary\n"+
       "-place_holder <string>  which vocab item is the place holder (default is " + null_sym->name() + " )\n"+
       "-show_cost              show cost of matching path\n"+
       "-o <string>             output file\n"+
       "-p <int>                'beam' width\n"+
       "Either:\n"+
       "-i <float>              insertion cost\n"+
       "-d <float>              deletion cost\n"+
       "-s <float>              substitution cost\n"+
       "Or:\n"+
       "-cost_matrix <string>   file containing cost matrix\n",
			files, al);

    out_file = al.present("-o") ? al.val("-o") : (EST_String)"-";

    if (al.present("-vocab"))
	load_vocab(al.val("-vocab"));
    else
    {
	cerr << argv[0] << ": no vocab file specified" << endl;
	exit(-1);
    }

    if (al.present("-p"))
	prune_width = al.ival("-p");
	    
    if (al.present("-cost_matrix"))
    {
	if(al.present("-i") || al.present("-d")  || al.present("-s") )
	{
	    cerr << "Can't have ins/del/subs costs as well as matrix !" << endl;
	    exit(-1);
	}
	distance_measure="matrix";
	cost_matrix.load(al.val("-cost_matrix"));

	if(al.present("-place_holder"))
	    null_sym->set_name(al.val("-place_holder"));

	if(StrVector_index(vocab,null_sym->name()) < 0)
	{
	    cerr << "The place holder symbol '" << null_sym->name();
	    cerr << "' is not in the vocbulary !" << endl;
	    exit(-1);
	}

	if(cost_matrix.num_columns() != vocab.length())
	{
	    cerr << "Cost matrix number of columns must match vocabulary size !" << endl;
	    exit(-1);
	}
	if(cost_matrix.num_rows() != vocab.length())
	{
	    cerr << "Cost matrix number of rows must match vocabulary size !" << endl;
	    exit(-1);
	}

    }
    else if(al.present("-i") && al.present("-d") && al.present("-s") )
    {
	insertion_cost = al.fval("-i");
	deletion_cost = al.fval("-d");
	substitution_cost = al.fval("-s");
    }
    else
    {
	cerr << "Must give either ins/del/subs costs or cost matrix !" << endl;
	exit(-1);
    }


    if(al.present("-show_cost"))
	show_cost=TRUE;

    if(files.length() != 2)
    {
	cerr << "Must give 2 patterns !" << endl;
	exit(-1);
    }

    StringtoStrList(files(files.head()),pattern1_l," ");
    StringtoStrList(files(files.head()->next()),pattern2_l," ");

    EST_Utterance utt;
    path1 = utt.create_relation("Lexical");
    path2 = utt.create_relation("Surface");
    match = utt.create_relation("Match");

    EST_Litem *p;
    for(p=pattern1_l.head();p != 0; p=p->next())
    {
	if( StrVector_index(vocab,pattern1_l(p)) < 0)
	{
	    cerr << pattern1_l(p) << " is not in the vocabulary !" << endl;
	    exit(1);
	}
	EST_Item new_item;
	new_item.set_name(pattern1_l(p));
	path1->append(&new_item);
    }

    for(p=pattern2_l.head();p != 0; p=p->next())
    {
	if( StrVector_index(vocab,pattern2_l(p)) < 0)
	{
	    cerr << pattern2_l(p) << " is not in the vocabulary !" << endl;
	    exit(1);
	}
	EST_Item new_item;
	new_item.set_name(pattern2_l(p));
	path2->append(&new_item);
    }

    // to do : check all items in two patterns are in vocab
    // .....


//    cerr << "MATCHING..." << endl;
    if(!dp_match(*path1,*path2,*match,
		local_cost,local_prune,null_sym))
//	cerr << "OK !" << endl;
//    else
	cerr << "No match could be found." << endl;


    utt.save(out_file);


    return 0;
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
	    vocab_l.append(ts.get().string());

    ts.close();

    StrList_to_StrVector(vocab_l,vocab);
}


float local_cost(const EST_Item *s1, const EST_Item *s2)
{
    // N.B. cost is not necessarily  zero for matching symbols

    //cerr << "lcf " << s1 << "," << s2 << endl;

    // otherwise cost is either insertion cost, or cost_matrix value
    if(distance_measure == "simple")
    {
	if(s1->name() == s2->name())
	    return 0;
	else
	{
	    if(s1 == null_sym)
		return insertion_cost;
	    else if(s2 == null_sym)
		return deletion_cost;
	    else
		return substitution_cost;
	}
    }

    //cerr << "Cost of replacing " << s1 << " with " << s2 << " is ";
    //cerr <<  cost_matrix(StrVector_index(vocab,s1),StrVector_index(vocab,s2)) << endl;

    return cost_matrix(StrVector_index(vocab,s1->name()),
		       StrVector_index(vocab,s2->name()));

}

bool local_prune(const int i, const int j,
		 const int max_i, const int max_j)
{

    // keep it simple : 
    // if we stray too far from the diagonal - PRUNE !

    float scale = (float)max_i / (float)max_j;

    float near_j = (float)i / scale;
    float near_i = (float)j * scale;

    /*
    cerr << "prune  i: " << i << " " << near_i - (float)i 
	<< " " << abs(near_i - (float)i)<< endl;
    cerr << "prune  j: " << j << " " << near_j - (float)j 
	<< " " << abs(near_j - (float)j) << endl;
   */

    if(  (abs((int)(near_i - (float)i)) > prune_width) ||
       (abs((int)(near_j - (float)j)) > prune_width) )
    {
	//cerr << "lpf " << i << "," << j << " true" << endl;
	return TRUE;
    }

    return FALSE;

}


/**@name Examples

<para>
Align two symbol sequences:
</para>

<para>
<screen>
$ dp -vocab vocab.file "a b c" "a b d c" -i 1 -d 2 -s 3
</screen>
</para>

<para>
where vocab.file contains "a b c d"
</para>

<para>
Or, using a full cost matrix:
</para>

<para>
<screen>
$ dp -vocab vocab2.file -cost_matrix foo "a b c" "a b d c"
</screen>
</para>

<para>
where vocab2.file contains "a b c d <null>" and the file foo contains:
</para>

<para>
<screen>
<para>0 3 3 3 2</para>
<para>3 0 3 3 2</para>
<para>3 3 0 3 2</para>
<para>3 3 3 0 2</para>
<para>1 1 1 1 0</para>
</screen>
</para>

<para> Each row of foo shows the cost of replacing an input symbol
with each symbol in the vocabulary to match an output symbol. Each row
corresponds to an item in the vocabulary (in the order they appear in
the vocabulary file). In the example, replacing 'a' with 'a' costs 0,
replacing 'a' with any of 'b' 'c' or 'd' costs 3 (a substitution), and
replacing 'a' with the place holder symbol 'null' costs 2 (a
deletion). The cost of replacing 'null' with anything other than
'null' costs 1 (an insertion).  The costs of 1,2 and 3 used here are
only for illustration. The cost matrix meed not have the form above -
for example, replacing 'a' with 'a' need not cost 0. The entries in
foo are read as floats.  </para>

*/
//@{
//@}
