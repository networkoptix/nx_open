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
/*                     Date   :  January 1998                            */
/*-----------------------------------------------------------------------*/
/*  Ordinary least squares                                               */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST_Wagon.h"
#include "EST_multistats.h"
#include "EST_cmd_line.h"

static void load_ols_data(EST_FMatrix &X, EST_FMatrix &Y, WDataSet &d);
static int ols_main(int argc, char **argv);


/** @name <command>ols</command> <emphasis>Train linear regression model</emphasis>
    @id ols-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**
 */

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main(int argc, char **argv)
{
    return ols_main(argc,argv);
}

static int ols_main(int argc, char **argv)
{
    // Top level function loads in sample data and finds coefficients
    EST_Option al;
    EST_StrList files;
    EST_String ofile = "-";
    WDataSet dataset,test_dataset;
    EST_FMatrix coeffs;
    EST_FMatrix X,Y,Xtest,Ytest;
    LISP ignores = NIL;

    parse_command_line
	(argc, argv,
       EST_String("[options]\n")+
       "Summary: Linear Regression by ordinary least squares (defaults in {})\n"+
	 "-desc <ifile>     Field description file\n"+
	 "-data <ifile>     Datafile, one vector per line\n"+
	 "-test <ifile>     Datafile, for testing\n"+
	 "-robust           Robust, may take longer\n"+
	 "-stepwise         Order the features by contribution,\n"+
	 "                  implies robust.\n"+
	 "-swlimit <float> {0.0}\n"+
	 "                  Percentage necessary improvement for stepwise\n"+
	 "-quiet            No summary\n"+
	 "-o      <ofile>   \n"+
	 "-output <ofile>   Output file for coefficients\n"+
	 "-ignore <string>  Filename or bracket list of fields to ignore\n",
	 files, al);


    if (al.present("-output"))
	ofile = al.val("-output");
    if (al.present("-o"))
	ofile = al.val("-o");

    siod_init();

    if (al.present("-ignore"))
    {
        EST_String ig = al.val("-ignore");
        if (ig[0] == '(')
            ignores = read_from_string(ig);
        else
            ignores = vload(ig,1);
    }

    // Load in the data
    if (!al.present("-desc"))
    {
	cerr << "ols: no description file specified\n";
	return -1;
    }
    else
    {
	dataset.load_description(al.val("-desc"),ignores);
        dataset.ignore_non_numbers();
    }
    if (!al.present("-data"))
    {
	cerr << "ols: no data file specified\n";
	return -1;
    }
    else
	wgn_load_dataset(dataset,al.val("-data"));
    if (al.present("-test"))
    {
	test_dataset.load_description(al.val("-desc"),ignores);
        test_dataset.ignore_non_numbers();
	wgn_load_dataset(test_dataset,al.val("-test"));
	load_ols_data(Xtest,Ytest,test_dataset);
    }
    else
	// No test data specified so use training data
	load_ols_data(Xtest,Ytest,dataset);

    load_ols_data(X,Y,dataset);

    if (al.present("-stepwise"))
    {
	EST_StrList names;
	float swlimit = al.fval("-swlimit");
        EST_IVector included;
        int i;

	names.append("Intercept");
	for (i=1; i < dataset.width(); i++)
	    names.append(dataset.feat_name(i));

        included.resize(X.num_columns());
        included[0] = TRUE;  // always guarantee interceptor
        for (i=1; i<included.length(); i++)
        {
            if (dataset.ignore(i) == TRUE)
                included.a_no_check(i) = OLS_IGNORE;
            else
                included.a_no_check(i) = FALSE;
        }

	if (!stepwise_ols(X,Y,names,swlimit,coeffs,Xtest,Ytest,included))
	{
	    cerr << "OLS: failed stepwise ols" << endl;
	    return -1;
	}
    }
    else if (al.present("-robust"))
    {
        EST_IVector included;
        int i;

        included.resize(X.num_columns());
        included[0] = TRUE;  // always guarantee interceptor
        for (i=1; i<included.length(); i++)
        {
            if (dataset.ignore(i) == TRUE)
                included.a_no_check(i) = OLS_IGNORE;
            else
                included.a_no_check(i) = TRUE;
        }

	if (!robust_ols(X,Y,included,coeffs))
	{
	    cerr << "OLS: failed robust ols" << endl;
	    return -1;
	}
    }
    else if (!ols(X,Y,coeffs))
    {
	cerr << "OLS: failed no pseudo_inverse" << endl;
	return -1;
    }

    if (coeffs.save(ofile) != write_ok)
    {
	cerr << "OLS: failed to save coefficients in \"" << ofile << "\"" 
	    << endl;
	return -1;
    }

    if (!al.present("-quiet"))
    {
	EST_FMatrix pred;
	float cor,rmse;

	ols_apply(Xtest,coeffs,pred);
	ols_test(Ytest,pred,cor,rmse);

	printf(";; RMSE %f Correlation is %f\n",rmse,cor);
    }

    return 0;
}

static void load_ols_data(EST_FMatrix &X, EST_FMatrix &Y, WDataSet &d)
{
    EST_Litem *p;
    int n,m;

    X.resize(d.length(),d.width());
    Y.resize(d.length(),1);

    for (n=0,p=d.head(); p != 0; p=p->next(),n++)
    {
	Y.a_no_check(n,0) = d(p)->get_flt_val(0);
	X.a_no_check(n,0) = 1;
	for (m=1; m < d.width(); m++)
        {
            if (d.ignore(m))
            {
                X.a_no_check(n,m) = 0;
            }
            else
                X.a_no_check(n,m) = d(p)->get_flt_val(m);
        }
    }

}
