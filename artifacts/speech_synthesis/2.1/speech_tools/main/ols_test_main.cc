/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1998                            */
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
/*  A program for testing a OLS                                          */
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

static int ols_test_main(int argc, char **argv);
static void load_ols_data(EST_FMatrix &X, EST_FMatrix &Y, WDataSet &d);

/** @name <command>ols_test</command> <emphasis>Test linear regression model</emphasis>
    @id ols-test-manual
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

    ols_test_main(argc,argv);

    exit(0);
    return 0;
}

static int ols_test_main(int argc, char **argv)
{
    // Top level function sets up data and creates a tree
    EST_Option al;
    EST_StrList files;
    EST_FMatrix X,Y,coeffs;
    WDataSet dataset;
    EST_String outfile;

    parse_command_line
	(argc, argv,
       EST_String("[options]\n")+
	 "ols_test  <options>\n"+
	 "program to test OLS on data\n"+
	 "-desc <ifile>     Field description file\n"+
	 "-data <ifile>     Datafile, one vector per line\n"+
	 "-coeffs <ifile>   File containing OLS coefficients\n"+
	 "-predict          Predict for each vector returning value\n"+
	 "-o <ofile>        File to save output in\n",
	 files, al);

    siod_init();

    if (al.present("-desc"))
    {
	dataset.load_description(al.val("-desc"),NIL);
    }
    else
    {
	cerr << argv[0] << ": no description file specified" << endl;
	exit(-1);
    }

    if (coeffs.load(al.val("-coeffs")) != format_ok)
    {
	cerr << argv[0] << ": no coefficients file specified" << endl;
	exit(-1);
    }

    if (al.present("-data"))
	wgn_load_dataset(dataset,al.val("-data"));
    else
    {
	cerr << argv[0] << ": no data file specified" << endl;
	exit(-1);
    }

    if (al.present("-o"))
	outfile = al.val("-o");
    else
	outfile = "-";

    EST_FMatrix pred;
    float cor,rmse;

    load_ols_data(X,Y,dataset);
    ols_apply(X,coeffs,pred);
    if (ols_test(Y,pred,cor,rmse))
	printf(";; RMSE %f Correlation is %f\n",rmse,cor);
    else
	printf(";; varation too small RMSE %f but no correlation\n",rmse);
    if (al.present("-o") || al.present("-predict"))
	pred.save(outfile);

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
	Y(n,0) = d(p)->get_flt_val(0);
	X(n,0) = 1;
	for (m=1; m < d.width(); m++)
	    X(n,m) = d(p)->get_flt_val(m);
    }

}
