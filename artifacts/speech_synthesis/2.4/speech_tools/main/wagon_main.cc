/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                     Copyright (c) 1996-2006                           */
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
/*                     Author :  Alan W Black                            */
/*                     Date   :  May 1996                                */
/*-----------------------------------------------------------------------*/
/*  A Classification and Regression Tree (CART) Program                  */
/*  A basic implementation of many of the techniques in                  */
/*  Briemen et al. 1984                                                  */
/*                                                                       */
/*  Added decision list support, Feb 1997                                */
/*                                                                       */
/*  Added vector support for Clustergen 2005/2006                        */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST_Wagon.h"
#include "EST_cmd_line.h"

enum wn_strategy_type {wn_decision_list, wn_decision_tree};

static wn_strategy_type wagon_type = wn_decision_tree;

static int wagon_main(int argc, char **argv);

/** @name <command>wagon</command> <emphasis>CART building program</emphasis>
    @id wagon_manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**
wagon is used to build CART tress from feature data, its basic
features include:

<itemizedlist>
<listitem><para>both decisions trees and decision lists are supported</para></listitem>
<listitem><para>predictees can be discrete or continuous</para></listitem>
<listitem><para>input features may be discrete or continuous</para></listitem>
<listitem><para>many options for controlling tree building</para>
<itemizedlist>
<listitem><para>fixed stop value</para></listitem>
<listitem><para>balancing</para></listitem>
<listitem><para>held-out data and pruning</para></listitem>
<listitem><para>stepwise use of input features</para></listitem>
<listitem><para>choice of optimization criteria correct/entropy (for 
classification and rmse/correlation (for regression)</para></listitem>
</itemizedlist>
</listitem>
</itemizedlist>

A detailed description of building CART models can be found in the
<link linkend="cart-overview">CART model overview</link> section.

*/

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}

int main(int argc, char **argv)
{

    wagon_main(argc,argv);

    exit(0);
    return 0;
}

static int set_Vertex_Feats(EST_Track &wgn_VertexFeats,
                            EST_String &wagon_track_features)
{
    int i,s=0,e;
    EST_TokenStream ts;

    for (i=0; i<wgn_VertexFeats.num_channels(); i++)
        wgn_VertexFeats.a(0,i) = 0.0;

    ts.open_string(wagon_track_features);
    ts.set_WhiteSpaceChars(",- ");
    ts.set_PunctuationSymbols("");
    ts.set_PrePunctuationSymbols("");
    ts.set_SingleCharSymbols("");

    while (!ts.eof())
    {
        EST_Token &token = ts.get();
        const EST_String ws = (const char *)token.whitespace();
        if (token == "all")
        {
            for (i=0; i<wgn_VertexFeats.num_channels(); i++)
                wgn_VertexFeats.a(0,i) = 1.0;
            break;
        } else if ((ws == ",") || (ws == ""))
        {
            s = atoi(token.string());
            wgn_VertexFeats.a(0,s) = 1.0;
        } else if (ws == "-")
        {
            if (token == "")
                e = wgn_VertexFeats.num_channels()-1;
            else
                e = atoi(token.string());
            for (i=s; i<=e && i<wgn_VertexFeats.num_channels(); i++)
                wgn_VertexFeats.a(0,i) = 1.0;
        } else
        {
            printf("wagon: track_feats invalid: %s at position %d\n",
                   (const char *)wagon_track_features,
                   ts.filepos());
            exit(-1);
        }
    }

    return 0;
}

static int wagon_main(int argc, char **argv)
{
    // Top level function sets up data and creates a tree
    EST_Option al;
    EST_StrList files;
    EST_String wgn_oname;
    ostream *wgn_coutput = 0;
    float stepwise_limit = 0;
    int feats_start=0, feats_end=0;
    int i;

    parse_command_line
	(argc, argv,
	 EST_String("[options]\n") +
	 "Summary: CART building program\n"+
	 "-desc <ifile>     Field description file\n"+
	 "-data <ifile>     Datafile, one vector per line\n"+
	 "-stop <int> {50}  Minimum number of examples for leaf nodes\n"+
	 "-test <ifile>     Datafile to test tree on\n"+
	 "-frs <float> {10} Float range split, number of partitions to\n"+
	 "                  split a float feature range into\n"+
	 "-dlist            Build a decision list (rather than tree)\n"+
	 "-dtree            Build a decision tree (rather than list) default\n"+
	 "-output <ofile>   \n"+
	 "-o <ofile>        File to save output tree in\n"+
	 "-distmatrix <ifile>\n"+
	 "                  A distance matrix for clustering\n"+
	 "-track <ifile>\n"+
         "                  track for vertex indices\n"+
	 "-track_start <int>\n"+
         "                  start channel vertex indices\n"+
	 "-track_end <int>\n"+
         "                  end (inclusive) channel for vertex indices\n"+
	 "-track_feats <string>\n"+
         "                  Track features to use, comma separated list\n"+
         "                  with feature numbers and/or ranges, 0 start\n"+
	 "-unittrack <ifile>\n"+
         "                  track for unit start and length in vertex track\n"+
	 "-quiet            No questions printed during building\n"+
	 "-verbose          Lost of information printing during build\n"+
	 "-predictee <string>\n"+
	 "                  name of field to predict (default is first field)\n"+
	 "-ignore <string>\n"+
	 "                  Filename or bracket list of fields to ignore\n"+
	 "-count_field <string>\n"+
	 "                  Name of field containing count weight for samples\n"+
	 "-stepwise         Incrementally find best features\n"+
	 "-swlimit <float> {0.0}\n"+
	 "                  Percentage necessary improvement for stepwise,\n"+
	 "                  may be negative.\n"+
	 "-swopt <string>   Parameter to optimize for stepwise, for \n"+
	 "                  classification options are correct or entropy\n"+
	 "                  for regression options are rmse or correlation\n"+
	 "                  correct and correlation are the defaults\n"+
	 "-balance <float>  For derived stop size, if dataset at node, divided\n"+
	 "                  by balance is greater than stop it is used as stop\n"+
	 "                  if balance is 0 (default) always use stop as is.\n"+
         "-vertex_output <string> Output <mean> or <best> of cluster\n"+
	 "-held_out <int>   Percent to hold out for pruning\n"+
	 "-heap <int> {210000}\n"+
	 "              Set size of Lisp heap, should not normally need\n"+
	 "              to be changed from its default, only with *very*\n"+
	 "              large description files (> 1M)\n"+
	 "-noprune          No (same class) pruning required\n",
		       files, al);

    if (al.present("-held_out"))
	wgn_held_out = al.ival("-held_out");
    if (al.present("-balance"))
	wgn_balance = al.fval("-balance");
    if ((!al.present("-desc")) || ((!al.present("-data"))))
    {
	cerr << argv[0] << ": missing description and/or datafile" << endl;
	cerr << "use -h for description of arguments" << endl;
    }

    if (al.present("-quiet"))
	wgn_quiet = TRUE;
    if (al.present("-verbose"))
	wgn_verbose = TRUE;

    if (al.present("-stop"))
	wgn_min_cluster_size = atoi(al.val("-stop"));
    if (al.present("-noprune"))
	wgn_prune = FALSE;
    if (al.present("-predictee"))
	wgn_predictee_name = al.val("-predictee");
    if (al.present("-count_field"))
	wgn_count_field_name = al.val("-count_field");
    if (al.present("-swlimit"))
	stepwise_limit = al.fval("-swlimit");
    if (al.present("-frs"))   // number of partitions to try in floats
	wgn_float_range_split = atof(al.val("-frs"));
    if (al.present("-swopt"))
	wgn_opt_param = al.val("-swopt");
    if (al.present("-vertex_output"))
	wgn_vertex_output = al.val("-vertex_output");
    if (al.present("-output") || al.present("-o"))
    {
	if (al.present("-o"))
	    wgn_oname = al.val("-o");
	else
	    wgn_oname = al.val("-output");
	wgn_coutput = new ofstream(wgn_oname);
	if (!(*wgn_coutput))
	{
	    cerr << "Wagon: can't open file \"" << wgn_oname <<
		"\" for output " << endl;
	    exit(-1);
	}
    }
    else
	wgn_coutput = &cout;
    if (al.present("-distmatrix"))
    {
	if (wgn_DistMatrix.load(al.val("-distmatrix")) != 0)
	{
	    cerr << "Wagon: failed to load Distance Matrix from \"" <<
		al.val("-distmatrix") << "\"\n" << endl;
	    exit(-1);
	}
    }
    if (al.present("-dlist"))
	wagon_type = wn_decision_list;

    WNode *tree;
    float score;
    LISP ignores = NIL;

    siod_init(al.ival("-heap"));

    if (al.present("-ignore"))
    {
	EST_String ig = al.val("-ignore");
	if (ig[0] == '(')
	    ignores = read_from_string(ig);
	else
	    ignores = vload(ig,1);
    }
    // Load in the data
    wgn_load_datadescription(al.val("-desc"),ignores);
    wgn_load_dataset(wgn_dataset,al.val("-data"));
    if (al.present("-distmatrix") &&
	(wgn_DistMatrix.num_rows() < wgn_dataset.length()))
    {
	cerr << "wagon: distance matrix is smaller than number of training elements\n";
	exit(-1);
    }
    else if (al.present("-track"))
    {
        wgn_VertexTrack.load(al.val("-track"));
        wgn_VertexFeats.resize(1,wgn_VertexTrack.num_channels());
        for (i=0; i<wgn_VertexFeats.num_channels(); i++)
            wgn_VertexFeats.a(0,i) = 1.0;
    }

    if (al.present("-track_start"))
    {
        feats_start = al.ival("-track_start");
        if ((feats_start < 0) ||
            (feats_start > wgn_VertexTrack.num_channels()))
        {
            printf("wagon: track_start invalid: %d out of %d channels\n",
                   feats_start,
                   wgn_VertexTrack.num_channels());
            exit(-1);
        }
        for (i=0; i<feats_start; i++)
            wgn_VertexFeats.a(0,i) = 0.0; /* don't do feats up to start */
            
    }

    if (al.present("-track_end"))
    {
        feats_end = al.ival("-track_end");
        if ((feats_end < feats_start) ||
            (feats_end > wgn_VertexTrack.num_channels()))
        {
            printf("wagon: track_end invalid: %d between start %d out of %d channels\n",
                   feats_end,
                   feats_start,
                   wgn_VertexTrack.num_channels());
            exit(-1);
        }
        for (i=feats_end+1; i<wgn_VertexTrack.num_channels(); i++)
            wgn_VertexFeats.a(0,i) = 0.0; /* don't do feats after end */
    }
    if (al.present("-track_feats"))
    {   /* overrides start and end numbers */
        EST_String wagon_track_features = al.val("-track_feats");
        set_Vertex_Feats(wgn_VertexFeats,wagon_track_features);
    }

    //    printf("Track feats\n");
    //    for (i=0; i<wgn_VertexTrack.num_channels(); i++)
    //        if (wgn_VertexFeats.a(0,i) > 0.0)
    //            printf("%d ",i);
    //    printf("\n");

    if (al.present("-unittrack"))
    {   /* contains two features, a start and length.  start indexes */
        /* into VertexTrack to the first vector in the segment */
        wgn_UnitTrack.load(al.val("-unittrack"));
    }

    if (al.present("-test"))
	wgn_load_dataset(wgn_test_dataset,al.val("-test"));

    // Build and test the model 
    if (al.present("-stepwise"))
	tree = wagon_stepwise(stepwise_limit);
    else if (wagon_type == wn_decision_tree)
	tree = wgn_build_tree(score);  // default operation
    else if (wagon_type == wn_decision_list)
	// dlist is printed with build_dlist rather than returned
	tree = wgn_build_dlist(score,wgn_coutput);
    else
    {
	cerr << "Wagon: unknown operation, not tree or list" << endl;
	exit(-1);
    }

    if (tree != 0)
    {
	*wgn_coutput << *tree;
	summary_results(*tree,wgn_coutput);
    }

    if (wgn_coutput != &cout)
	delete wgn_coutput;
    return 0;
}

/** @name Building Trees

To build a decision tree (or list) Wagon requires data and a description
of it.  A data file consists a set of samples, one per line each
consisting of the same set of features.   Features may be categorial
or continuous.  By default the first feature is the predictee and the
others are used as predictors.  A typical data file will look like
this
</para>
<para>
<screen>
0.399 pau sh  0   0     0 1 1 0 0 0 0 0 0 
0.082 sh  iy  pau onset 0 1 0 0 1 1 0 0 1
0.074 iy  hh  sh  coda  1 0 1 0 1 1 0 0 1
0.048 hh  ae  iy  onset 0 1 0 1 1 1 0 1 1
0.062 ae  d   hh  coda  1 0 0 1 1 1 0 1 1
0.020 d   y   ae  coda  2 0 1 1 1 1 0 1 1
0.082 y   ax  d   onset 0 1 0 1 1 1 1 1 1
0.082 ax  r   y   coda  1 0 0 1 1 1 1 1 1
0.036 r   d   ax  coda  2 0 1 1 1 1 1 1 1
...
</screen>
</para>
<para>
The data may come from any source, such as the festival script 
dumpfeats which allows the creation of such files easily from utterance
files.  
</para><para>
In addition to a data file a description file is also require that 
gives a name and a type to each of the features in the datafile.
For the above example it would look like
</para><para>
<screen>
((segment_duration float)
 ( name  aa ae ah ao aw ax ay b ch d dh dx eh el em en er ey f g 
    hh ih iy jh k l m n nx ng ow oy p r s sh t th uh uw v w y z zh pau )
 ( n.name 0 aa ae ah ao aw ax ay b ch d dh dx eh el em en er ey f g 
    hh ih iy jh k l m n nx ng ow oy p r s sh t th uh uw v w y z zh pau )
 ( p.name 0 aa ae ah ao aw ax ay b ch d dh dx eh el em en er ey f g 
    hh ih iy jh k l m n nx ng ow oy p r s sh t th uh uw v w y z zh pau )
 (position_type 0 onset coda)
 (pos_in_syl float)
 (syl_initial 0 1)
 (syl_final   0 1)
 (R:Sylstructure.parent.R:Syllable.p.syl_break float)
 (R:Sylstructure.parent.syl_break float)
 (R:Sylstructure.parent.R:Syllable.n.syl_break float)
 (R:Sylstructure.parent.R:Syllable.p.stress 0 1)
 (R:Sylstructure.parent.stress 0 1)
 (R:Sylstructure.parent.R:Syllable.n.stress 0 1)
)
</screen>
</para><para>
The feature names are arbitrary, but as they appear in the generated
trees is most useful if the trees are to be used in prediction of
an utterance that the names are features and/or pathnames.  
</para><para>
Wagon can be used to build a tree with such files with the command
<screen>
wagon -data feats.data -desc fest.desc -stop 10 -output feats.tree
</screen>
A test data set may also be given which must match the given data description.
If specified the built tree will be tested on the test set and results
on that will be presented on completion, without a test set the
results are given with respect to the training data.  However in
stepwise case the test set is used in the multi-level training process
thus it cannot be considered as true test data and more reasonable 
results should found on applying the generate tree to truly
held out data (via the program wagon_test).

*/

//@}
