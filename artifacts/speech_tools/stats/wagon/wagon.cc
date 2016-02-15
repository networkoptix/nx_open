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
/*                     Date   :  May 1996                                */
/*-----------------------------------------------------------------------*/
/*  A Classification and Regression Tree (CART) Program                  */
/*  A basic implementation of many of the techniques in                  */
/*  Briemen et al. 1984                                                  */
/*                                                                       */
/*  Added decision list support, Feb 1997                                */
/*  Added stepwise use of features, Oct 1997                             */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST_Token.h"
#include "EST_FMatrix.h"
#include "EST_multistats.h"
#include "EST_Wagon.h"
#include "EST_math.h"

Discretes wgn_discretes;

WDataSet wgn_dataset;
WDataSet wgn_test_dataset;
EST_FMatrix wgn_DistMatrix;
EST_Track wgn_VertexTrack;
EST_Track wgn_VertexFeats;
EST_Track wgn_UnitTrack;

int wgn_min_cluster_size = 50;
int wgn_held_out = 0;
int wgn_prune = TRUE;
int wgn_quiet = FALSE;
int wgn_verbose = FALSE;
int wgn_count_field = -1;
EST_String wgn_count_field_name = "";
int wgn_predictee = 0;
EST_String wgn_predictee_name = "";
float wgn_float_range_split = 10;
float wgn_balance = 0;
EST_String wgn_opt_param = "";
EST_String wgn_vertex_output = "mean";
EST_String wgn_vertex_otype = "mean";

static float do_summary(WNode &tree,WDataSet &ds,ostream *output);
static float test_tree_float(WNode &tree,WDataSet &ds,ostream *output);
static float test_tree_class(WNode &tree,WDataSet &ds,ostream *output);
static float test_tree_cluster(WNode &tree,WDataSet &dataset, ostream *output);
static float test_tree_vector(WNode &tree,WDataSet &dataset,ostream *output);
static float test_tree_trajectory(WNode &tree,WDataSet &dataset,ostream *output);
static float test_tree_ols(WNode &tree,WDataSet &dataset,ostream *output);
static int wagon_split(int margin,WNode &node);
static WQuestion find_best_question(WVectorVector &dset);
static void construct_binary_ques(int feat,WQuestion &test_ques);
static float construct_float_ques(int feat,WQuestion &ques,WVectorVector &ds);
static float construct_class_ques(int feat,WQuestion &ques,WVectorVector &ds);
static void wgn_set_up_data(WVectorVector &data,const WVectorList &ds,int held_out,int in);
static WNode *wagon_stepwise_find_next_best(float &bscore,int &best_feat);

Declare_TList_T(WVector *, WVectorP)

Declare_TVector_Base_T(WVector *,NULL,NULL,WVectorP)

#if defined(INSTANTIATE_TEMPLATES)
// Instantiate class
#include "../base_class/EST_TList.cc"
#include "../base_class/EST_TVector.cc"

Instantiate_TList_T(WVector *, WVectorP)

Instantiate_TVector(WVector *)

#endif

void wgn_load_datadescription(EST_String fname,LISP ignores)
{
    // Load field description for a file
    wgn_dataset.load_description(fname,ignores);
    wgn_test_dataset.load_description(fname,ignores);
}

void wgn_load_dataset(WDataSet &dataset,EST_String fname)
{
    // Read the data set from a filename.  One vector per line
    // Assume all numbers are numbers and non-nums are categorical
    EST_TokenStream ts;
    WVector *v;
    int nvec=0,i;

    if (ts.open(fname) == -1)
	wagon_error(EST_String("unable to open data file \"")+
		    fname+"\"");
    ts.set_PunctuationSymbols("");
    ts.set_PrePunctuationSymbols("");
    ts.set_SingleCharSymbols("");

    for ( ;!ts.eof(); )
    {
	v = new WVector(dataset.width());
	i = 0;
	do 
	{
	    int type = dataset.ftype(i);
	    if ((type == wndt_float) || 
                (type == wndt_ols) ||
                (wgn_count_field == i))
	    {
		// need to ensure this is not NaN or Infinity
		float f = atof(ts.get().string());
		if (isfinite(f))
		    v->set_flt_val(i,f);
		else
		{
		    cout << fname << ": bad float " << f
			<< " in field " <<
			dataset.feat_name(i) << " vector " << 
			    dataset.samples() << endl;
		    v->set_flt_val(i,0.0);
		}
	    }
	    else if (type == wndt_binary)
		v->set_int_val(i,atoi(ts.get().string()));
	    else if (type == wndt_cluster)  /* index into distmatrix */
		v->set_int_val(i,atoi(ts.get().string()));
	    else if (type == wndt_vector)   /* index into VertexTrack */
		v->set_int_val(i,atoi(ts.get().string()));
	    else if (type == wndt_trajectory) /* index to index and length */
            {   /* a number pointing to a vector in UnitTrack that */
                /* has an idex into VertexTrack and a number of Vertices */
                /* Thus if its 15, UnitTrack.a(15,0) is the start frame in */
                /* VertexTrack and UnitTrack.a(15,1) is the number of */
                /* frames in the unit                                 */
		v->set_int_val(i,atoi(ts.get().string()));
            }
	    else if (type == wndt_ignore)
	    {
		ts.get();  // skip it
		v->set_int_val(i,0);
	    }
	    else // should check the different classes 
	    {
		EST_String s = ts.get().string();
		int n = wgn_discretes.discrete(type).name(s); 
		if (n == -1)
		{
		    cout << fname << ": bad value " << s << " in field " <<
			dataset.feat_name(i) << " vector " << 
			    dataset.samples() << endl;
		    n = 0;
		}
		v->set_int_val(i,n);
	    }
	    i++;
	}
	while (!ts.eoln() && i<dataset.width());
	nvec ++;
	if (i != dataset.width())
	{
	    wagon_error(fname+": data vector "+itoString(nvec)+" contains "
			+itoString(i)+" parameters instead of "+
			itoString(dataset.width()));
	}
	if (!ts.eoln())
	{
	    cerr << fname << ": data vector " << nvec << 
		" contains too many parameters instead of " 
		<< dataset.width() << endl;
	    wagon_error(EST_String("extra parameter(s) from ")+
			ts.peek().string());
	}
	dataset.append(v);
    }

    cout << "Dataset of " << dataset.samples() << " vectors of " <<
	dataset.width() << " parameters from: " << fname << endl;
    ts.close();
}

float summary_results(WNode &tree,ostream *output)
{
    if (wgn_test_dataset.samples() != 0)
	return do_summary(tree,wgn_test_dataset,output);
    else
	return do_summary(tree,wgn_dataset,output);
}

static float do_summary(WNode &tree,WDataSet &ds,ostream *output)
{
    if (wgn_dataset.ftype(wgn_predictee) == wndt_cluster)
	return test_tree_cluster(tree,ds,output);
    else if (wgn_dataset.ftype(wgn_predictee) == wndt_vector)
	return test_tree_vector(tree,ds,output);
    else if (wgn_dataset.ftype(wgn_predictee) == wndt_trajectory)
	return test_tree_trajectory(tree,ds,output);
    else if (wgn_dataset.ftype(wgn_predictee) == wndt_ols)
	return test_tree_ols(tree,ds,output);
    else if (wgn_dataset.ftype(wgn_predictee) >= wndt_class)
	return test_tree_class(tree,ds,output);
    else
	return test_tree_float(tree,ds,output);
}

WNode *wgn_build_tree(float &score)
{
    // Build init node and split it while reducing the impurity
    WNode *top = new WNode();
    int margin = 0;

    wgn_set_up_data(top->get_data(),wgn_dataset,wgn_held_out,TRUE);

    margin = 0;
    wagon_split(margin,*top);  // recursively split data;

    if (wgn_held_out > 0)
    {
	wgn_set_up_data(top->get_data(),wgn_dataset,wgn_held_out,FALSE);
	top->held_out_prune();
    }
	
    if (wgn_prune)
	top->prune();

    score = summary_results(*top,0);

    return top;
}

static void wgn_set_up_data(WVectorVector &data,const WVectorList &ds,int held_out,int in)
{
    // Set data ommitting held_out percent if in is true
    // or only including 100-held_out percent if in is false
    int i,j;
    EST_Litem *d;

    // Make it definitely big enough
    data.resize(ds.length());
    
    for (j=i=0,d=ds.head(); d != 0; d=d->next(),j++)
    {
	if ((in) && ((j%100) >= held_out))
	    data[i++] = ds(d);
//	else if ((!in) && ((j%100 < held_out)))
//	    data[i++] = ds(d);
	else if (!in)
	    data[i++] = ds(d);
//	if ((in) && (j < held_out))
//	    data[i++] = ds(d);	    
//	else if ((!in) && (j >=held_out))
//	    data[i++] = ds(d);	    
    }
    // make it the actual size, but don't reset values
    data.resize(i,1);  
}

static float test_tree_class(WNode &tree,WDataSet &dataset,ostream *output)
{
    // Test tree against data to get summary of results
    EST_StrStr_KVL pairs;
    EST_StrList lex;
    EST_Litem *p;
    EST_String predict,real;
    WNode *pnode;
    double H=0,prob;
    int i,type;
    float correct=0,total=0, count=0;

    float bcorrect=0, bpredicted=0, bactual=0;
    float precision=0, recall=0;

    for (p=dataset.head(); p != 0; p=p->next())
    {
	pnode = tree.predict_node((*dataset(p)));
	predict = (EST_String)pnode->get_impurity().value();
	if (wgn_count_field == -1)
	    count = 1.0;
	else
	    count = dataset(p)->get_flt_val(wgn_count_field);
	prob = pnode->get_impurity().pd().probability(predict);
	H += (log(prob))*count;
	type = dataset.ftype(wgn_predictee);
	real = wgn_discretes[type].name(dataset(p)->get_int_val(wgn_predictee));
	
	if (wgn_opt_param == "B_NB_F1")
	  {
	    //cout << real << " " << predict << endl;
	    if (real == "B")
	      bactual +=count;
	    if (predict == "B")
	      {
		bpredicted += count;
		if (real == predict)
		  bcorrect += count;
	      }
	    //	    cout <<bactual << " " << bpredicted << " " << bcorrect << endl;
	  }
	if (real == predict)
	    correct += count;
	total += count;
	pairs.add_item(real,predict,1);
    }
    for (i=0; i<wgn_discretes[dataset.ftype(wgn_predictee)].length(); i++)
	lex.append(wgn_discretes[dataset.ftype(wgn_predictee)].name(i));

    const EST_FMatrix &m = confusion(pairs,lex);
    
    if (output != NULL)
    {
	print_confusion(m,pairs,lex);  // should be to output not stdout
	*output << ";; entropy " << (-1*(H/total)) << " perplexity " <<
	    pow(2.0,(-1*(H/total))) << endl;
    }

    
    // Minus it so bigger is better 
    if (wgn_opt_param == "entropy")
	return -pow(2.0,(-1*(H/total)));
    else if(wgn_opt_param == "B_NB_F1")
      {
	if(bpredicted == 0)
	  precision = 1;
	else
	  precision = bcorrect/bpredicted;
	if(bactual == 0)
	  recall = 1;
	else
	  recall = bcorrect/bactual;
	float fmeasure = 0;
	if((precision+recall) !=0)
	  fmeasure = 2* (precision*recall)/(precision+recall);
	cout<< "F1 :" << fmeasure << " Prec:" << precision << " Rec:" << recall << " B-Pred:" << bpredicted << " B-Actual:" << bactual << " B-Correct:" << bcorrect << endl;
	return fmeasure;
      }
    else
	return (float)correct/(float)total;
}

static float test_tree_vector(WNode &tree,WDataSet &dataset,ostream *output)
{
    // Test tree against data to get summary of results VECTOR
    // distance is calculated in zscores (as the values in vector may
    // have quite different ranges 
    WNode *leaf;
    EST_Litem *p;
    float predict, actual;
    EST_SuffStats x,y,xx,yy,xy,se,e;
    EST_SuffStats b;
    int i,j,pos;
    double cor,error;
    double count;
    EST_Litem *pp;

    for (p=dataset.head(); p != 0; p=p->next())
    {
	leaf = tree.predict_node((*dataset(p)));
	pos = dataset(p)->get_int_val(wgn_predictee);
        for (j=0; j<wgn_VertexFeats.num_channels(); j++)
            if (wgn_VertexFeats.a(0,j) > 0.0)
            {
                b.reset();
                for (pp=leaf->get_impurity().members.head(); pp != 0; pp=pp->next())
                {
                    i = leaf->get_impurity().members.item(pp);
                    b += wgn_VertexTrack.a(i,j);
                }
                predict = b.mean();
                actual = wgn_VertexTrack.a(pos,j);
                if (wgn_count_field == -1)
                    count = 1.0;
                else
                    count = dataset(p)->get_flt_val(wgn_count_field);
                x.cumulate(predict,count);
                y.cumulate(actual,count);
                /* Normalized the error by the standard deviation */
                if (b.stddev() == 0)
                    error = predict-actual;
                else
                    error = (predict-actual)/b.stddev();
                error = predict-actual; /* awb_debug */
                se.cumulate((error*error),count);
                e.cumulate(fabs(error),count);
                xx.cumulate(predict*predict,count);
                yy.cumulate(actual*actual,count);
                xy.cumulate(predict*actual,count);
            }
    }

    // Pearson's product moment correlation coefficient
//    cor = (xy.mean() - (x.mean()*y.mean()))/
//	(sqrt(xx.mean()-(x.mean()*x.mean())) *
//	 sqrt(yy.mean()-(y.mean()*y.mean())));
    // Because when the variation is X is very small we can
    // go negative, thus cause the sqrt's to give FPE
    double v1 = xx.mean()-(x.mean()*x.mean());
    double v2 = yy.mean()-(y.mean()*y.mean());

    double v3 = v1*v2;

    if (v3 <= 0)
	// happens when there's very little variation in x
	cor = 0;
    else
	cor = (xy.mean() - (x.mean()*y.mean()))/ sqrt(v3);
    
    if (output != NULL)
    {
	if (output != &cout)   // save in output file
	    *output 
		<< ";; RMSE " << ftoString(sqrt(se.mean()),4,1)
		<< " Correlation is " << ftoString(cor,4,1)
		<< " Mean (abs) Error " << ftoString(e.mean(),4,1)
		<< " (" << ftoString(e.stddev(),4,1) << ")" << endl;
	
	cout << "RMSE " << ftoString(sqrt(se.mean()),4,1)
	    << " Correlation is " << ftoString(cor,4,1)
	    << " Mean (abs) Error " << ftoString(e.mean(),4,1)
	    << " (" << ftoString(e.stddev(),4,1) << ")" << endl;
    }

    if (wgn_opt_param == "rmse")
	return -sqrt(se.mean());  // * -1 so bigger is better 
    else
	return cor;  // should really be % variance, I think
}

static float test_tree_trajectory(WNode &tree,WDataSet &dataset,ostream *output)
{
    // Test tree against data to get summary of results TRAJECTORY
    // distance is calculated in zscores (as the values in vector may
    // have quite different ranges)
    // NOT WRITTEN YET
    WNode *leaf;
    EST_Litem *p;
    float predict, actual;
    EST_SuffStats x,y,xx,yy,xy,se,e;
    EST_SuffStats b;
    int i,j,pos;
    double cor,error;
    double count;
    EST_Litem *pp;

    for (p=dataset.head(); p != 0; p=p->next())
    {
	leaf = tree.predict_node((*dataset(p)));
	pos = dataset(p)->get_int_val(wgn_predictee);
        for (j=0; j<wgn_VertexFeats.num_channels(); j++)
            if (wgn_VertexFeats.a(0,j) > 0.0)
            {
                b.reset();
                for (pp=leaf->get_impurity().members.head(); pp != 0; pp=pp->next())
                {
                    i = leaf->get_impurity().members.item(pp);
                    b += wgn_VertexTrack.a(i,j);
                }
                predict = b.mean();
                actual = wgn_VertexTrack.a(pos,j);
                if (wgn_count_field == -1)
                    count = 1.0;
                else
                    count = dataset(p)->get_flt_val(wgn_count_field);
                x.cumulate(predict,count);
                y.cumulate(actual,count);
                /* Normalized the error by the standard deviation */
                if (b.stddev() == 0)
                    error = predict-actual;
                else
                    error = (predict-actual)/b.stddev();
                error = predict-actual; /* awb_debug */
                se.cumulate((error*error),count);
                e.cumulate(fabs(error),count);
                xx.cumulate(predict*predict,count);
                yy.cumulate(actual*actual,count);
                xy.cumulate(predict*actual,count);
            }
    }

    // Pearson's product moment correlation coefficient
//    cor = (xy.mean() - (x.mean()*y.mean()))/
//	(sqrt(xx.mean()-(x.mean()*x.mean())) *
//	 sqrt(yy.mean()-(y.mean()*y.mean())));
    // Because when the variation is X is very small we can
    // go negative, thus cause the sqrt's to give FPE
    double v1 = xx.mean()-(x.mean()*x.mean());
    double v2 = yy.mean()-(y.mean()*y.mean());

    double v3 = v1*v2;

    if (v3 <= 0)
	// happens when there's very little variation in x
	cor = 0;
    else
	cor = (xy.mean() - (x.mean()*y.mean()))/ sqrt(v3);
    
    if (output != NULL)
    {
	if (output != &cout)   // save in output file
	    *output 
		<< ";; RMSE " << ftoString(sqrt(se.mean()),4,1)
		<< " Correlation is " << ftoString(cor,4,1)
		<< " Mean (abs) Error " << ftoString(e.mean(),4,1)
		<< " (" << ftoString(e.stddev(),4,1) << ")" << endl;
	
	cout << "RMSE " << ftoString(sqrt(se.mean()),4,1)
	    << " Correlation is " << ftoString(cor,4,1)
	    << " Mean (abs) Error " << ftoString(e.mean(),4,1)
	    << " (" << ftoString(e.stddev(),4,1) << ")" << endl;
    }

    if (wgn_opt_param == "rmse")
	return -sqrt(se.mean());  // * -1 so bigger is better 
    else
	return cor;  // should really be % variance, I think
}

static float test_tree_cluster(WNode &tree,WDataSet &dataset,ostream *output)
{
    // Test tree against data to get summary of results for cluster trees
    WNode *leaf;
    int real;
    int right_cluster=0;
    EST_SuffStats ranking, meandist;
    EST_Litem *p;

    for (p=dataset.head(); p != 0; p=p->next())
    {
	leaf = tree.predict_node((*dataset(p)));
	real = dataset(p)->get_int_val(wgn_predictee);
	meandist += leaf->get_impurity().cluster_distance(real);
	right_cluster += leaf->get_impurity().in_cluster(real);
	ranking += leaf->get_impurity().cluster_ranking(real);
    }

    if (output != NULL)
    {
	// Want number in right class, mean distance in sds, mean ranking
	if (output != &cout)   // save in output file
	    *output << ";; Right cluster " << right_cluster << " (" <<
		(int)(100.0*(float)right_cluster/(float)dataset.length()) << 
		    "%) mean ranking " << ranking.mean() << " mean distance "
			<< meandist.mean() << endl;
	cout << "Right cluster " << right_cluster << " (" <<
	    (int)(100.0*(float)right_cluster/(float)dataset.length()) << 
		"%) mean ranking " << ranking.mean() << " mean distance "
		    << meandist.mean() << endl;
    }

    return 10000-meandist.mean();  // this doesn't work but I tested it
}

static float test_tree_float(WNode &tree,WDataSet &dataset,ostream *output)
{
    // Test tree against data to get summary of results FLOAT
    EST_Litem *p;
    float predict,real;
    EST_SuffStats x,y,xx,yy,xy,se,e;
    double cor,error;
    double count;

    for (p=dataset.head(); p != 0; p=p->next())
    {
	predict = tree.predict((*dataset(p)));
	real = dataset(p)->get_flt_val(wgn_predictee);
	if (wgn_count_field == -1)
	    count = 1.0;
	else
	    count = dataset(p)->get_flt_val(wgn_count_field);
	x.cumulate(predict,count);
	y.cumulate(real,count);
	error = predict-real;
	se.cumulate((error*error),count);
	e.cumulate(fabs(error),count);
	xx.cumulate(predict*predict,count);
	yy.cumulate(real*real,count);
	xy.cumulate(predict*real,count);
    }

    // Pearson's product moment correlation coefficient
//    cor = (xy.mean() - (x.mean()*y.mean()))/
//	(sqrt(xx.mean()-(x.mean()*x.mean())) *
//	 sqrt(yy.mean()-(y.mean()*y.mean())));
    // Because when the variation is X is very small we can
    // go negative, thus cause the sqrt's to give FPE
    double v1 = xx.mean()-(x.mean()*x.mean());
    double v2 = yy.mean()-(y.mean()*y.mean());

    double v3 = v1*v2;

    if (v3 <= 0)
	// happens when there's very little variation in x
	cor = 0;
    else
	cor = (xy.mean() - (x.mean()*y.mean()))/ sqrt(v3);
    
    if (output != NULL)
    {
	if (output != &cout)   // save in output file
	    *output 
		<< ";; RMSE " << ftoString(sqrt(se.mean()),4,1)
		<< " Correlation is " << ftoString(cor,4,1)
		<< " Mean (abs) Error " << ftoString(e.mean(),4,1)
		<< " (" << ftoString(e.stddev(),4,1) << ")" << endl;
	
	cout << "RMSE " << ftoString(sqrt(se.mean()),4,1)
	    << " Correlation is " << ftoString(cor,4,1)
	    << " Mean (abs) Error " << ftoString(e.mean(),4,1)
	    << " (" << ftoString(e.stddev(),4,1) << ")" << endl;
    }

    if (wgn_opt_param == "rmse")
	return -sqrt(se.mean());  // * -1 so bigger is better 
    else
	return cor;  // should really be % variance, I think
}

static float test_tree_ols(WNode &tree,WDataSet &dataset,ostream *output)
{
    // Test tree against data to get summary of results OLS
    EST_Litem *p;
    WNode *leaf;
    float predict,real;
    EST_SuffStats x,y,xx,yy,xy,se,e;
    double cor,error;
    double count;

    for (p=dataset.head(); p != 0; p=p->next())
    {
	leaf = tree.predict_node((*dataset(p)));
        // do ols to get predict;
        predict = 0.0;
	real = dataset(p)->get_flt_val(wgn_predictee);
	if (wgn_count_field == -1)
	    count = 1.0;
	else
	    count = dataset(p)->get_flt_val(wgn_count_field);
	x.cumulate(predict,count);
	y.cumulate(real,count);
	error = predict-real;
	se.cumulate((error*error),count);
	e.cumulate(fabs(error),count);
	xx.cumulate(predict*predict,count);
	yy.cumulate(real*real,count);
	xy.cumulate(predict*real,count);
    }

    // Pearson's product moment correlation coefficient
//    cor = (xy.mean() - (x.mean()*y.mean()))/
//	(sqrt(xx.mean()-(x.mean()*x.mean())) *
//	 sqrt(yy.mean()-(y.mean()*y.mean())));
    // Because when the variation is X is very small we can
    // go negative, thus cause the sqrt's to give FPE
    double v1 = xx.mean()-(x.mean()*x.mean());
    double v2 = yy.mean()-(y.mean()*y.mean());

    double v3 = v1*v2;

    if (v3 <= 0)
	// happens when there's very little variation in x
	cor = 0;
    else
	cor = (xy.mean() - (x.mean()*y.mean()))/ sqrt(v3);
    
    if (output != NULL)
    {
	if (output != &cout)   // save in output file
	    *output 
		<< ";; RMSE " << ftoString(sqrt(se.mean()),4,1)
		<< " Correlation is " << ftoString(cor,4,1)
		<< " Mean (abs) Error " << ftoString(e.mean(),4,1)
		<< " (" << ftoString(e.stddev(),4,1) << ")" << endl;
	
	cout << "RMSE " << ftoString(sqrt(se.mean()),4,1)
	    << " Correlation is " << ftoString(cor,4,1)
	    << " Mean (abs) Error " << ftoString(e.mean(),4,1)
	    << " (" << ftoString(e.stddev(),4,1) << ")" << endl;
    }

    if (wgn_opt_param == "rmse")
	return -sqrt(se.mean());  // * -1 so bigger is better 
    else
	return cor;  // should really be % variance, I think
}

static int wagon_split(int margin, WNode &node)
{
    // Split given node (if possible)
    WQuestion q;
    WNode *l,*r;

    node.set_impurity(WImpurity(node.get_data()));
    q = find_best_question(node.get_data());

/*    printf("q.score() %f impurity %f\n",
	   q.get_score(),
	   node.get_impurity().measure()); */

    double impurity_measure = node.get_impurity().measure();   
    double question_score = q.get_score();       

    if ((question_score < WGN_HUGE_VAL) && 
        (question_score < impurity_measure))

    {
	// Ok its worth a split
	l = new WNode();
	r = new WNode();
	wgn_find_split(q,node.get_data(),l->get_data(),r->get_data());
	node.set_subnodes(l,r);
	node.set_question(q);
	if (wgn_verbose)
	{
	    int i;
	    for (i=0; i < margin; i++)
		cout << " ";
	    cout << q << endl;
	}
	margin++;
	wagon_split(margin,*l);
	margin++;
	wagon_split(margin,*r);
	margin--;
	return TRUE;
    }
    else
    {
	if (wgn_verbose)
	{
	    int i;
	    for (i=0; i < margin; i++)
		cout << " ";
	    cout << "stopped samples: " << node.samples() << " impurity: " 
		<< node.get_impurity() << endl;
	}
	margin--;
	return FALSE;
    }
}

void wgn_find_split(WQuestion &q,WVectorVector &ds,
		    WVectorVector &y,WVectorVector &n)
{
    int i, iy, in;

    y.resize(q.get_yes());
    n.resize(q.get_no());
    
    for (iy=in=i=0; i < ds.n(); i++)
	if (q.ask(*ds(i)) == TRUE)
	    y[iy++] = ds(i);
	else
	    n[in++] = ds(i);

}

static WQuestion find_best_question(WVectorVector &dset)
{
    //  Ask all possible questions and find the best one
    int i;
    float bscore,tscore;
    WQuestion test_ques, best_ques;

    bscore = tscore = WGN_HUGE_VAL;
    best_ques.set_score(bscore);
    // test each feature with each possible question
    for (i=0;i < wgn_dataset.width(); i++)
    {
	if ((wgn_dataset.ignore(i) == TRUE) ||
	    (i == wgn_predictee))
	    tscore = WGN_HUGE_VAL;     // ignore this feature this time
	else if (wgn_dataset.ftype(i) == wndt_binary)
	{
	    construct_binary_ques(i,test_ques);
	    tscore = wgn_score_question(test_ques,dset);
	}
	else if (wgn_dataset.ftype(i) == wndt_float)
	{
	    tscore = construct_float_ques(i,test_ques,dset);
	}
	else if (wgn_dataset.ftype(i) == wndt_ignore)
	    tscore = WGN_HUGE_VAL;    // always ignore this feature
#if 0
	// This doesn't work reasonably 
	else if (wgn_csubset && (wgn_dataset.ftype(i) >= wndt_class))
	{
	    wagon_error("subset selection temporarily deleted");
	    tscore = construct_class_ques_subset(i,test_ques,dset);
	}
#endif
	else if (wgn_dataset.ftype(i) >= wndt_class)
	    tscore = construct_class_ques(i,test_ques,dset);
	if (tscore < bscore)
	{
	    best_ques = test_ques;
	    best_ques.set_score(tscore);
	    bscore = tscore;
	}
    }

    return best_ques;
}

static float construct_class_ques(int feat,WQuestion &ques,WVectorVector &ds)
{
    // Find out which member of a class gives the best split
    float tscore,bscore = WGN_HUGE_VAL;
    int cl;
    WQuestion test_q;

    test_q.set_fp(feat);
    test_q.set_oper(wnop_is);
    ques = test_q;
    
    for (cl=0; cl < wgn_discretes[wgn_dataset.ftype(feat)].length(); cl++)
    {
	test_q.set_operand1(EST_Val(cl));
	tscore = wgn_score_question(test_q,ds);
	if (tscore < bscore)
	{
	    ques = test_q;
	    bscore = tscore;
	}
    }

    return bscore;
}

#if 0
static float construct_class_ques_subset(int feat,WQuestion &ques,
					 WVectorVector &ds)
{
    // Find out which subset of a class gives the best split.
    // We first measure the subset of the data for each member of 
    // of the class.  Then order those splits.  Then go through finding
    // where the best split of that ordered list is.  This is described
    // on page 247 of Breiman et al.
    float tscore,bscore = WGN_HUGE_VAL;
    LISP l;
    int cl;

    ques.set_fp(feat);
    ques.set_oper(wnop_is);
    float *scores = new float[wgn_discretes[wgn_dataset.ftype(feat)].length()];
    
    // Only do it for exists values
    for (cl=0; cl < wgn_discretes[wgn_dataset.ftype(feat)].length(); cl++)
    {
	ques.set_operand(flocons(cl));
	scores[cl] = wgn_score_question(ques,ds);
    }

    LISP order = sort_class_scores(feat,scores);
    if (order == NIL)
	return WGN_HUGE_VAL;
    if (siod_llength(order) == 1)
    {   // Only one so we know the best "split"
	ques.set_oper(wnop_is);
	ques.set_operand(car(order));
	return scores[get_c_int(car(order))];
    }

    ques.set_oper(wnop_in);
    LISP best_l = NIL;
    for (l=cdr(order); CDR(l) != NIL; l = cdr(l))
    {
	ques.set_operand(l);
	tscore = wgn_score_question(ques,ds);
	if (tscore < bscore)
	{
	    best_l = l;
	    bscore = tscore;
	}

    }

    if (best_l != NIL)
    {
	if (siod_llength(best_l) == 1)
	{
	    ques.set_oper(wnop_is);
	    ques.set_operand(car(best_l));
	}
	else if (equal(cdr(order),best_l) != NIL)
	{
	    ques.set_oper(wnop_is);
	    ques.set_operand(car(order));
	}
	else
	{
	    cout << "Found a good subset" << endl;
	    ques.set_operand(best_l);
	}
    }
    return bscore;
}

static LISP sort_class_scores(int feat,float *scores)
{
    // returns sorted list of (non WGN_HUGE_VAL) items
    int i;
    LISP items = NIL;
    LISP l;

    for (i=0; i < wgn_discretes[wgn_dataset.ftype(feat)].length(); i++)
    {
	if (scores[i] != WGN_HUGE_VAL)
	{
	    if (items == NIL)
		items = cons(flocons(i),NIL);
	    else
	    {
		for (l=items; l != NIL; l=cdr(l))
		{
		    if (scores[i] < scores[get_c_int(car(l))])
		    {
			CDR(l) = cons(car(l),cdr(l));
			CAR(l) = flocons(i);
			break;
		    }
		}
		if (l == NIL)
		    items = l_append(items,cons(flocons(i),NIL));
	    }
	}
    }
    return items;
}
#endif

static float construct_float_ques(int feat,WQuestion &ques,WVectorVector &ds)
{
    // Find out a split of the range that gives the best score 
    // Naively does this by partitioning the range into float_range_split slots
    float tscore,bscore = WGN_HUGE_VAL;
    int d, i;
    float p;
    WQuestion test_q;
    float max,min,val,incr;

    test_q.set_fp(feat);
    test_q.set_oper(wnop_lessthan);
    ques = test_q;

    min = max = ds(0)->get_flt_val(feat);  /* set up some value */
    for (d=0; d < ds.n(); d++)
    {
	val = ds(d)->get_flt_val(feat);
	if (val < min)
	    min = val;
	else if (val > max)
	    max = val;
    }
    if (max == min)  // we're pure
	return WGN_HUGE_VAL;
    incr = (max-min)/wgn_float_range_split;  
    // so do float_range-1 splits
    /* We calculate this based on the number splits, not the increments, */
    /* becuase incr can be so small it doesn't increment p */
    for (i=0,p=min+incr; i < wgn_float_range_split; i++,p += incr )
    {
	test_q.set_operand1(EST_Val(p));
	tscore = wgn_score_question(test_q,ds);
	if (tscore < bscore)
	{
	    ques = test_q;
	    bscore = tscore;
	}
    }

    return bscore;
}

static void construct_binary_ques(int feat,WQuestion &test_ques)
{
    // construct a question.  Not sure about this in general
    // of course continuous/categorical features will require different
    // rule and non-binary ones will require some test point

    test_ques.set_fp(feat);
    test_ques.set_oper(wnop_binary);
    test_ques.set_operand1(EST_Val(""));
}

static float score_question_set(WQuestion &q, WVectorVector &ds, int ignorenth)
{
    // score this question as a possible split by finding
    // the sum of the impurities when ds is split with this question
    WImpurity y,n;
    int d, num_yes, num_no;
    float count;
    WVector *wv;

    num_yes = num_no = 0;
    y.data = &ds;
    n.data = &ds;
    for (d=0; d < ds.n(); d++)
    {
	if ((ignorenth < 2) ||
	    (d%ignorenth != ignorenth-1))
	{
	    wv = ds(d);
	    if (wgn_count_field == -1)
		count = 1.0;
	    else 
		count = (*wv)[wgn_count_field];

	    if (q.ask(*wv) == TRUE)
	    {
		num_yes++;
                if (wgn_dataset.ftype(wgn_predictee) == wndt_ols)
                    y.cumulate(d,count);  // note the sample number not value
                else
                    y.cumulate((*wv)[wgn_predictee],count);
	    }
	    else
	    {
		num_no++;
                if (wgn_dataset.ftype(wgn_predictee) == wndt_ols)
                    n.cumulate(d,count);  // note the sample number not value
                else
                    n.cumulate((*wv)[wgn_predictee],count);
	    }
	}
    }

    q.set_yes(num_yes);
    q.set_no(num_no);

    int min_cluster;

    if ((wgn_balance == 0.0) ||
	(ds.n()/wgn_balance < wgn_min_cluster_size))
	min_cluster = wgn_min_cluster_size;
    else 
	min_cluster = (int)(ds.n()/wgn_balance);

    if ((y.samples() < min_cluster) ||
	(n.samples() < min_cluster))
	return WGN_HUGE_VAL;

    float ym,nm,bm;
    //    printf("awb_debug score_question_set X %f Y %f\n",
    //    y.samples(), n.samples());
    ym = y.measure();
    nm = n.measure();
    bm = ym + nm;

    /*    cout << q << endl;
    printf("test question y %f n %f b %f\n",
    ym, nm, bm); */

    return bm/2.0;
}

float wgn_score_question(WQuestion &q, WVectorVector &ds)
{
    // This level of indirection was introduced for later expansion

    return score_question_set(q,ds,1);
}

WNode *wagon_stepwise(float limit)
{
    // Find the best single features and incrementally add features
    // that best improve result until it doesn't improve.
    // This is basically to automate what Kurt was doing in building
    // trees, he then automated it in PERL and as it seemed to work
    // I put it into wagon itself.
    // This can be pretty computationally intensive.
    WNode *best = 0,*new_best = 0;
    float bscore,best_score = -WGN_HUGE_VAL;
    int best_feat,i;
    int nf = 1;

    // Set all features to ignore
    for (i=0; i < wgn_dataset.width(); i++)
	wgn_dataset.set_ignore(i,TRUE);

    for (i=0; i < wgn_dataset.width(); i++)
    {
	if ((wgn_dataset.ftype(i) == wndt_ignore) || (i == wgn_predictee))
	{
	    // This skips the round not because this has anything to
	    // do with this feature being (user specified) ignored
	    // but because it indicates there is one less cycle that is
	    // necessary
	    continue;
	}
	new_best = wagon_stepwise_find_next_best(bscore,best_feat);

	if ((bscore - fabs(bscore * (limit/100))) <= best_score)
	{
	    // gone as far as we can
	    delete new_best;
	    break;
	}
	else
	{
	    best_score = bscore;
	    delete best;
	    best = new_best;
	    wgn_dataset.set_ignore(best_feat,FALSE);
	    if (!wgn_quiet)
	    {
		fprintf(stdout,"FEATURE    %d %s: %2.4f\n",
			nf,
			(const char *)wgn_dataset.feat_name(best_feat),
			best_score);
		fflush(stdout);
		nf++;
	    }
	}
    }

    return best;
}

static WNode *wagon_stepwise_find_next_best(float &bscore,int &best_feat)
{
    // Find which of the currently ignored features will best improve
    // the result
    WNode *best = 0;
    float best_score = -WGN_HUGE_VAL;
    int best_new_feat = -1;
    int i;

    for (i=0; i < wgn_dataset.width(); i++)
    {
	if (wgn_dataset.ftype(i) == wndt_ignore)
	    continue; // user wants me to ignore this completely
	else if (i == wgn_predictee) // can't use the answer
	    continue;
	else if (wgn_dataset.ignore(i) == TRUE)
	{
	    WNode *current;
	    float score;

	    // Allow this feature to participate
	    wgn_dataset.set_ignore(i,FALSE);
	    
	    current = wgn_build_tree(score);

	    if (score > best_score)
	    {
		best_score = score;
		delete best;
		best = current;
		best_new_feat = i;
//		fprintf(stdout,"BETTER FEATURE    %d %s: %2.4f\n",
//			i,
//			(const char *)wgn_dataset.feat_name(i),
//			best_score);
//		fflush(stdout);
	    }
	    else
		delete current;

	    // switch it off again
	    wgn_dataset.set_ignore(i,TRUE);
	}
    }

    bscore = best_score;
    best_feat = best_new_feat;
    return best;
}
