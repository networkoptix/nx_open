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
/*                 Authors:  Simon King                                  */
/*                 Date   :  July 1995                                   */
/*-----------------------------------------------------------------------*/
/*                 EST_Ngrammar test program                             */
/*                                                                       */
/*=======================================================================*/
#include "EST.h"
#include "EST_Ngrammar.h"


/** @name <command>ngram_test</command> <emphasis> Test n-gram language model </emphasis>
    @id ngram_test_manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**
ngram_test is for testing ngram models generated from
<link linkend=ngram-build-manual>ngram_build</link>.

<formalpara> <para> <title> How do we test an ngram model ?  </title>
</para>

<para> ngram_test will compute the entropy (or perplexity, see below)
of some test data, given an ngram model. The entropy gives a measure
of how likely the ngram model is to have generated the test
data. Entropy is defined (for a sliding-window type ngram) as:

\[H = -\frac{1}{Q} \sum_{i=1}^{Q} log P(w_i | w_{i-1}, w_{i-2},... w_{i-N+1}) \]

where \(Q\) is the number of words of test data and \(N\) is the order
of the ngram model. Perplexity is a more intuitive mease, defined as:

\[B = 2^H \]

The perplexity of an ngram model with vocabulary size V will be
between 1 and V. Low perplexity indicates a more predictable language,
and in speech recognition, a models with low perplexity on test data
(i.e. data NOT used to estimate the model in the first place)
typically give better accuracy recognition than models with higher 
perplexity (this is not guaranteed, however).

test_ngram works with non-sliding-window type models when the input
format is <parameter>ngram_per_line</parameter>.

</para>
</formalpara>

<formalpara>
<para><title>Input data format</title></para>
<para> The data input format options are the same as
<link linkend=ngram-build-manual>ngram_build</link>, as is the treatment of sentence start/end using
special tags. 
</para>
<para>

Note: To get meaningful entropy/perplexity figures, it is recommended that
you use the same data input format in both
<link linkend=ngram-build-manual>ngram_build</link> and <link linkend=ngram-test-manual>ngram_test</link>, and the treatment of
sentence start/end should be the same.
</para>
</formalpara>


@see ngram_build */
//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main(int argc, char **argv)
{
    //int order;
    EST_StrList files,script;
    EST_Option al, op;
    EST_String wordlist_file, script_file, in_file, format;
    EST_String prev_tag, prev_prev_tag, last_tag;
    EST_Litem *p;
    //EST_Ngrammar::representation_t representation = 
    //EST_Ngrammar::dense;

    EST_StrList wordlist;
    EST_Ngrammar ngrammar;
    bool per_file_stats=false;
    bool raw_stats=false;
    bool brief=false;
    EST_String input_format;

    double raw_entropy,count,entropy,perplexity,total_raw_H,total_count;
    total_count = 0;
    total_raw_H = 0;

    parse_command_line
    (argc, argv, 
     EST_String("[input file0] [input file1] ...\n")+
	 "-g <ifile>   grammar file (required)\n"+
	 "-w <ifile>   filename containing word list (required for some grammar formats)\n"+
	 "-S <ifile>   script file\n"+
         "-raw_stats   print unnormalised entropy and sample count\n"+
         "-brief       print results in brief format\n"+
         "-f           print stats for each file\n"+
   	 "\n"+
	 "-input_format <string>\n"+
         "             format of input data (default sentence_per_line)\n"+
	 "             may also be sentence_per_file, or ngram_per_line.\n"+
         "\n"+
	 "Pseudo-words :\n"+
	 "-prev_tag <string>\n"+
         "             tag before sentence start\n"+
	 "-prev_prev_tag <string>\n"+
         "             all words before 'prev_tag'\n"+
	 "-last_tag <string>\n"+
         "             after sentence end\n"+
	 "-default_tags\n"+
         "             use default tags of "+SENTENCE_START_MARKER+
			","+SENTENCE_END_MARKER+" and "+SENTENCE_END_MARKER+"\n"+
         "             respectively\n",
			files, al);


    if (al.present("-w"))
	wordlist_file = al.val("-w");
    else{
	wordlist_file = "";
    }

    if (al.present("-f"))
	per_file_stats = true;
    if (al.present("-input_format"))
	input_format = al.val("-input_format");
    else
	input_format = "sentence_per_line";

    if ( al.present("-raw_stats") || al.present("-r"))
	raw_stats = true;

    if ( al.present("-brief") || al.present("-b") )
	brief = true;


    if (al.present("-default_tags"))
    {	
	prev_tag = SENTENCE_START_MARKER;
	prev_prev_tag = SENTENCE_END_MARKER;
	last_tag = SENTENCE_END_MARKER;
    }
    
    if (al.present("-prev_tag"))
    {
	if (al.present("-default_tags"))
	    cerr << "test_ngram: WARNING : -prev_tag overrides -default_tags"
		<< endl;
	prev_tag = al.val("-prev_tag");
    }

    if (al.present("-prev_prev_tag"))
    {
	if (al.present("-default_tags"))
	    cerr << "test_ngram: WARNING : -prev_prev_tag overrides -default_tags" << endl;
	prev_prev_tag = al.val("-prev_prev_tag");
    }	
    
    if (al.present("-last_tag"))
    {
	if (al.present("-default_tags"))
	    cerr << "test_ngram: WARNING : -last_tag overrides -default_tags" << endl;
	last_tag = al.val("-last_tag");
    }

    if (   ( (prev_tag=="") ||  (prev_prev_tag=="") || (last_tag=="") )
	&& ( (prev_tag!="") ||  (prev_prev_tag!="") || (last_tag!="") )   )
    {
	cerr << "test_ngram: ERROR : if any tags are given, ALL must be given" << endl;
	exit(1);
    }


    // script
    if (al.present("-S"))
    {
	script_file = al.val("-S");
    
	if(load_StrList(script_file,script) != format_ok)
	{
	    cerr << "test_ngram: Could not read script from file " 
		<< script_file << endl;
	    exit(1);
	}
    }

    if (al.present("-g"))
	in_file = al.val("-g");
    else
    {
	cerr << "test_ngram: Must give a grammar filename using -g" << endl;
	exit(1);
    }

    // plus any files on command line
    // except file "-" unless there is no script
    if(script.head()==NULL)
	script += files;
    else
	for(p=files.head();p!=0;p=p->next())
	    if(files(p) != "-")
		script.append(files(p));

    if(script.head() == NULL)
    {
	cerr << "test_ngram: No test files given" << endl;
	exit(1);
    }

    if (wordlist_file != "")
    {
	// load wordlist
	if (load_StrList(wordlist_file,wordlist) != format_ok)
	{
	    cerr << "test_ngram: Could not read wordlist from file " << wordlist_file
		<< endl;
	    exit(1);
	}
    
	// load grammar using wordlist
	if (ngrammar.load(in_file,wordlist) != format_ok)
	{
	    cerr << "test_ngram: Failed to load grammar" << endl;
	    exit(1);
	}
    }
    else
    {
	if (ngrammar.load(in_file) != format_ok)
	{
	    cerr << "test_ngram: Failed to load grammar" << endl;
	    exit(1);
	}
    }

    if (!brief)
    {
	cout << "Ngram Test Results" << endl;
	cout << "==================" << endl;
    }

    for (p = script.head(); p; p = p->next())
    {
	// test each file
	if (test_stats(ngrammar,
		       script(p),
		       raw_entropy,count,
		       entropy,perplexity,
		       input_format,
		       prev_tag,
		       prev_prev_tag))
	{
	    total_raw_H += raw_entropy;
	    total_count += count;
	    
	    if(per_file_stats)
	    {
		if (brief)
		    cout << basename(script(p)) << " \t";
		else
		    cout << script(p) << endl;

		if(raw_stats)
		{
		    if (brief)
			 cout << raw_entropy << " " << count << " ";
		    else
		    {
			cout << " raw entropy " << raw_entropy << endl;
			cout << " count       " << count << endl;
		    }
		}
		
		if (brief)
		    cout << entropy << " " << perplexity << endl;
		else
		{
		    cout << " entropy     " << entropy << endl;
		    cout << " perplexity  " << perplexity << endl << endl;
		}
	    }
	}
	else
	{
	    cerr << "test_ngram: WARNING : file '" << script(p)
		<< "' could not be processed" << endl;
	}
	
    }
    if (total_count > 0)
    {
	if (!brief)
	    cout << "Summary for grammar " << in_file << endl;
	else
	    if (per_file_stats)
		cout << "summary \t";

	if(raw_stats)
	{
	    if (brief)
		cout << total_raw_H << " " << total_count << " ";
	    else
	    {
		cout << " raw entropy " << total_raw_H << endl;
		cout << " count       " << total_count << endl;
	    }
	}
	if (brief)
	{
	    cout << total_raw_H / total_count;
	    cout << " " << pow(2.0,total_raw_H / total_count);
	    cout << endl;
	}
	else
	{
	    cout << " entropy     " << total_raw_H / total_count << endl;
	    cout << " perplexity  " <<  pow(2.0,total_raw_H / total_count);
	    cout << endl;
	}
    }
    else
    {
	cerr << "test_ngram: No data processed" << endl;
    }
    
    // everything went okay
    return 0;
}


void override_lib_ops(EST_Option &a_list, EST_Option &al)
{
    (void)a_list;
    (void)al;
}

/** @name Hints

<title>I got a perplexity of Infinity - what went wrong ?</title>

A perplexity of Infinity means that at least one of the ngrams in your
test data had a probability of zero. Possible reasons for this include:

<itemizedlist>

<listitem><para>The training data had no examples of this ngram, and
you did not specify a floor for zero frequency ngrams in
\Ref{build_ngram} </para></listitem>
<listitem><para>You used differing input formats for \Ref{ngram_build}
and \Ref{ngram_test}. </para></listitem>
<listitem><para>You used differing sentence start/end treatments in
\Ref{ngram_build} and \Ref{ngram_test}. </para></listitem>
</itemizedlist>

*/

  //@{
  //@}

//@}
