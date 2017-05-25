/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                      Copyright (c) 1996,1997                          */
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
/*                     Date   :  October 1997                            */
/*-----------------------------------------------------------------------*/
/*  A program for testing a CART tree against data, also may be used to  */
/*  predict values using a tree and data                                 */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST_Wagon.h"
#include "EST_cutils.h"
#include "EST_multistats.h"
#include "EST_Token.h"
#include "EST_cmd_line.h"

static int wagon_test_main(int argc, char **argv);
static LISP find_feature_value(const char *feature, 
			       LISP vector, LISP description);
static LISP wagon_vector_predict(LISP tree, LISP vector, LISP description);
static LISP get_data_vector(EST_TokenStream &data, LISP description);
static void simple_predict(EST_TokenStream &data, FILE *output, 
			   LISP tree, LISP description, int all_info);
static void test_tree_class(EST_TokenStream &data, FILE *output, 
			    LISP tree, LISP description);
static void test_tree_float(EST_TokenStream &data, FILE *output, 
			    LISP tree, LISP description);

/** @name <command>wagon_test</command> <emphasis>Test CART models</emphasis>
    @id wagon-test-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**
Wagon_test is used to test CART models on feature data.

A detailed description of the CART model can be found in the
<link linkend="cart-overview">CART model overview</link> section.
*/

int main(int argc, char **argv)
{

    wagon_test_main(argc,argv);

    exit(0);
    return 0;
}

static int wagon_test_main(int argc, char **argv)
{
    // Top level function sets up data and creates a tree
    EST_Option al;
    EST_StrList files;
    LISP description,tree=NIL;;
    EST_TokenStream data;
    FILE *wgn_output;

    parse_command_line
	(argc, argv,
	 EST_String("<options>\n")+
	 "Summary: program to test CART models on data\n"+
	 "-desc <ifile>     Field description file\n"+
	 "-data <ifile>     Datafile, one vector per line\n"+
	 "-tree <ifile>     File containing CART tree\n"+
	 "-track <ifile>\n"+
         "                  track for vertex indices\n"+
	 "-predict          Predict for each vector returning full vector\n"+
	 "-predict_val      Predict for each vector returning just value\n"+
	 "-predictee <string>\n"+
	 "                  name of field to predict (default is first field)\n"+
	 "-heap <int> {210000}\n"+
	 "              Set size of Lisp heap, should not normally need\n"+
	 "              to be changed from its default\n"+
	 "-o <ofile>        File to save output in\n",
	 files, al);

    siod_init(al.ival("-heap"));

    if (al.present("-desc"))
    {
	gc_protect(&description);
	description = car(vload(al.val("-desc"),1));
    }
    else
    {
	cerr << argv[0] << ": no description file specified" << endl;
	exit(-1);
    }

    if (al.present("-tree"))
    {
	gc_protect(&tree);
	tree = car(vload(al.val("-tree"),1));
	if (tree == NIL)
	{
	    cerr << argv[0] << ": no tree found in \"" << al.val("-tree")
		<< "\"" << endl;
	    exit(-1);
	}
    }
    else
    {
	cerr << argv[0] << ": no tree file specified" << endl;
	exit(-1);
    }

    if (al.present("-data"))
    {
	if (data.open(al.val("-data")) != 0)
	{
	    cerr << argv[0] << ": can't open data file \"" << 
		al.val("-data") << "\" for input." << endl;
	    exit(-1);
	}
    }
    else
    {
	cerr << argv[0] << ": no data file specified" << endl;
	exit(-1);
    }

    if (al.present("-track"))
    {
        wgn_VertexTrack.load(al.val("-track"));
    }

    if (al.present("-o"))
    {
	if ((wgn_output = fopen(al.val("-o"),"w")) == NULL)
	{
	    cerr << argv[0] << ": can't open output file \"" <<
		al.val("-o") << "\"" << endl;
	}
    }
    else
	wgn_output = stdout;

    if (al.present("-predictee"))
    {
	LISP l;
	int i;
	wgn_predictee_name = al.val("-predictee");
	for (l=description,i=0; l != NIL; l=cdr(l),i++)
	    if (streq(wgn_predictee_name,get_c_string(car(car(l)))))
	    {
		wgn_predictee = i;
		break;
	    }
	if (l==NIL)
	{
	    cerr << argv[0] << ": predictee \"" << wgn_predictee <<
		"\" not in description\n"; 
	}
    }
    const char *predict_type =
	get_c_string(car(cdr(siod_nth(wgn_predictee,description))));

    if (al.present("-predict"))
	simple_predict(data,wgn_output,tree,description,FALSE);
    else if (al.present("-predict_val"))
	simple_predict(data,wgn_output,tree,description,TRUE);
    else if (streq(predict_type,"float") ||
	     streq(predict_type,"int"))
	test_tree_float(data,wgn_output,tree,description);
#if 0
    else if (streq(predict_type,"vector"))
	test_tree_vector(data,wgn_output,tree,description);
#endif
    else
	test_tree_class(data,wgn_output,tree,description);

    if (wgn_output != stdout)
	fclose(wgn_output);
    data.close();
    return 0;
}

static LISP get_data_vector(EST_TokenStream &data, LISP description)
{
    // read in one vector.  Should be terminated with an newline
    LISP v=NIL,d;

    if (data.eof())
	return NIL;

    for (d=description; d != NIL; d=cdr(d))
    {
	EST_Token t = data.get();
	
	if ((d != description) && (t.whitespace().contains("\n")))
	{
	    cerr << "wagon_test: unexpected newline within vector " <<
		t.row() << " wrong number of features" << endl;
	    siod_error();
	}
	if (streq(get_c_string(car(cdr(car(d)))),"float") ||
	    streq(get_c_string(car(cdr(car(d)))),"int"))
	    v = cons(flocons(atof(t.string())),v);
	else if ((streq(get_c_string(car(cdr(car(d)))),"_other_")) &&
		 (siod_member_str(t.string(),cdr(car(d))) == NIL))
	    v = cons(strintern("_other_"),v);
	else
	    v = cons(strintern(t.string()),v);
    }

    return reverse(v);
}

static void simple_predict(EST_TokenStream &data, FILE *output, 
			   LISP tree, LISP description, int all_info)
{
    LISP vector,predict;
    EST_String val;

    for (vector=get_data_vector(data,description); 
	 vector != NIL; vector=get_data_vector(data,description))
    {
	predict = wagon_vector_predict(tree,vector,description);
	if (all_info)
	    val = siod_sprint(car(reverse(predict)));
	else
	    val = siod_sprint(predict);
	fprintf(output,"%s\n",(const char *)val);
    }
}

static void test_tree_float(EST_TokenStream &data, FILE *output, 
			    LISP tree, LISP description)
{
    // Test tree against data to get summary of results FLOAT
    float predict_val,real_val;
    EST_SuffStats x,y,xx,yy,xy,se,e;
    double cor,error;
    LISP vector,predict;

    for (vector=get_data_vector(data,description); 
	 vector != NIL; vector=get_data_vector(data,description))
    {
	predict = wagon_vector_predict(tree,vector,description);
	predict_val = get_c_float(car(reverse(predict)));
	real_val = get_c_float(siod_nth(wgn_predictee,vector));
	x += predict_val;
	y += real_val;
	error = predict_val-real_val;
	se += error*error;
	e += fabs(error);
	xx += predict_val*predict_val;
	yy += real_val*real_val;
	xy += predict_val*real_val;
    }

    cor = (xy.mean() - (x.mean()*y.mean()))/
	(sqrt(xx.mean()-(x.mean()*x.mean())) *
	 sqrt(yy.mean()-(y.mean()*y.mean())));

    fprintf(output,";; RMSE %1.4f Correlation is %1.4f Mean (abs) Error %1.4f (%1.4f)\n",
	    sqrt(se.mean()),
	    cor,
	    e.mean(),
	    e.stddev());
}

static void test_tree_class(EST_TokenStream &data, FILE *output, 
			    LISP tree, LISP description)
{
    // Test tree against class data to get summary of results
    EST_StrStr_KVL pairs;
    EST_StrList lex;
    EST_String predict_class,real_class;
    LISP vector,w,predict;
    double H=0,Q=0,prob;
    (void)output;

    for (vector=get_data_vector(data,description); 
	 vector != NIL; vector=get_data_vector(data,description))
    {
	predict = wagon_vector_predict(tree,vector,description);
	predict_class = get_c_string(car(reverse(predict)));
	real_class = get_c_string(siod_nth(wgn_predictee,vector));
	prob = get_c_float(car(cdr(siod_assoc_str(real_class,
						  predict))));
	if (prob == 0)
	    H += log(0.000001);
	else
	    H += log(prob);
	Q ++;
	pairs.add_item(real_class,predict_class,1);
    }
    for (w=cdr(siod_nth(wgn_predictee,description)); w != NIL; w = cdr(w))
	lex.append(get_c_string(car(w)));

    const EST_FMatrix &m = confusion(pairs,lex);
    print_confusion(m,pairs,lex);
    fprintf(stdout,";; entropy %g perplexity %g\n",
	    (-1*(H/Q)),pow(2.0,(-1*(H/Q))));
}

#if 0
static void test_tree_vector(EST_TokenStream &data, FILE *output, 
                             LISP tree, LISP description)
{
    // Test tree against class data to get summary of results
    // Note we are talking about predicting vectors (a *bunch* of
    // numbers, not just a single class here)
    EST_StrStr_KVL pairs;
    EST_StrList lex;
    EST_String predict_class,real_class;
    LISP vector,w,predict;
    double H=0,Q=0,prob;
    (void)output;

    for (vector=get_data_vector(data,description); 
	 vector != NIL; vector=get_data_vector(data,description))
    {
	predict = wagon_vector_predict(tree,vector,description);
	predict_class = get_c_string(car(reverse(predict)));
	real_class = get_c_string(siod_nth(wgn_predictee,vector));
	prob = get_c_float(car(cdr(siod_assoc_str(real_class,
						  predict))));
	if (prob == 0)
	    H += log(0.000001);
	else
	    H += log(prob);
	Q ++;
	pairs.add_item(real_class,predict_class,1);
    }
    for (w=cdr(siod_nth(wgn_predictee,description)); w != NIL; w = cdr(w))
	lex.append(get_c_string(car(w)));

    const EST_FMatrix &m = confusion(pairs,lex);
    print_confusion(m,pairs,lex);
    fprintf(stdout,";; entropy %g perplexity %g\n",
	    (-1*(H/Q)),pow(2.0,(-1*(H/Q))));
}
#endif


static LISP wagon_vector_predict(LISP tree, LISP vector, LISP description)
{
    // Using the LISP tree, vector and description, do standard prediction

    if (cdr(tree) == NIL)
	return car(tree);

    LISP value = find_feature_value(wgn_ques_feature(car(tree)),
				    vector, description);
    
    if (wagon_ask_question(car(tree),value))
	// Yes answer
	return wagon_vector_predict(car(cdr(tree)),vector,description);
    else 
	// No answer
	return wagon_vector_predict(car(cdr(cdr(tree))),vector,description);
}

static LISP find_feature_value(const char *feature, 
			       LISP vector, LISP description)
{
    LISP v,d;

    for (v=vector,d=description; v != NIL; v=cdr(v),d=cdr(d))
	if (streq(feature,get_c_string(car(car(d)))))
	    return car(v);

    cerr << "wagon_test: can't find feature \"" << feature <<
	"\" in description" << endl;
    siod_error();
    return NIL;

}

/** @name Testing trees
<para>
Decision trees generated by wagon (or otherwise) can be applied
to and tested against data sets using this program.  This program
requires a data set which is in the same format as wagon (and
other programs) requires.  It also needs a dataset description
file naming the fields and given their type (see 
<link linkend="wagon-manual">the wagon manual</link> for a description
for the actual format.
<screen>
wagon_test -data feats.data -desc feats.desc -tree feats.tree
</screen>
This will simply uses the tree against each sample in the data
file and compare the predicted value with the actual value and
produce a summary of the result.  For categorial predictees a
percentage correct and confusion matrix is generated.  For continuous
values the root mean squared error (RMSE) and correlation between the
predicted values and the actual values is given.  
</para><para>
By default the predictee is the first field but may also be specified
on the command line.  The dataset may contain features which are not
used by the tree.
</para><para>
This program can also be used to generate output values for sampled
data.  In this case the sample data must still contain a "value" for
the predictee even if it is dummy.  The option
<command>-predict</command> will output the new sample vector with
the predicted value in place, and the option
<command>-predict_val</command> option will just output the value.
</para><para>
This program is specifically designed for testing purposes although it
can also just be used for prediction. It is probably more efficient
to use the Lisp function <command>wagon</command> or underlying
C++ function <command>wagon_predict()</command>.

*/

//@}
