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
/*                 EST_Ngrammar build program                            */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include "EST.h"
#include "EST_Ngrammar.h"
#include "EST_Pathname.h"



/** @name <command>ngram_build</command> <emphasis>Train n-gram language model</emphasis>
    @id ngram_build_manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**
ngram_build offers basic ngram language model estimation. 


<formalpara>
<para><title>Input data format</title></para>

<para> Two input formats are supported. In sentence_per_line format,
the program will deal with start and end of sentence (if required) by
using special vocabulary items specified by -prev_tag, -prev_prev_tag
and -last_tag. For example, the input sentence: </para>

<screen>
the cat sat on the mat
</screen>

would be treated as

<screen>
... prev_prev_tag prev_prev_tag prev_tag the cat sat on the mat last_tag
</screen>

where prev_prev_tag is the argument to -prev_prev_tag, and so on. A
default set of tag names is also available. This input format is only
useful for sliding-window type applications (e.g. language modelling
for speech recognition).

The second input format is ngram_per_line which is useful for either
non-sliding-window applications, or where the user requires an
alternative treatment of start/end of sentence to that provided
above. Now the input file simply contains a complete ngram per
line. For the same example as above (to build a trigram model) this
would be:

<para>
<screen>
prev_prev_tag prev_tag the
prev_tag the cat
the cat sat
cat sat on
sat on the
on the mat
the mat last_tag
</screen>
</para>

</formalpara>


<formalpara>
<para><title>Representation</title></para>

\[V^N\]

<para> The internal representation of the model becomes important for
higher values of N where, if V is the vocabulary size, \(V^N\) becomes
very large. In such cases, we cannot explicitly hold probabilities for
all possible ngrams, and a sparse representation must be used
(i.e. only non-zero probabilities are stored).</para> 
</formalpara>

<formalpara>
<para><title>Getting more robust probability estimates</title></para>
The common techniques for getting better estimates of the low/zero
frequency ngrams are provided: namely smoothing and backing-off</para>
</formalpara>

<formalpara>
<para><title>Testing an ngram model</title></para>
Use the <link linkend=ngram-test-manual>ngram_test</link> program.
</formalpara>

*/

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main(int argc, char **argv)
{
    int order;
    EST_StrList files;
    EST_Option al, op;
    EST_String wordlist_file,wordlist_file2, out_file, format;
    EST_String prev_tag(""), prev_prev_tag(""), last_tag("");
    EST_String input_format(""), oov_mode(""), oov_marker("");
    EST_Ngrammar::representation_t representation = 
	EST_Ngrammar::dense;

    EST_StrList wordlist,wordlist2;
    EST_Ngrammar ngrammar;
    bool trace=false;
    double floor=0.0;

    parse_command_line
    (argc, argv, 
     EST_String("[input file0] [input file1] ... -o [output file]\n")+
	 "-w <ifile>       filename containing word list (required)\n"+
	 "-p <ifile>       filename containing predictee word list\n"+
         "                  (default is to use wordlist given by -w)\n"+
	 "-order <int>     order, 1=unigram, 2=bigram etc. (default 2)\n"+
	 "-smooth <int>    Good-Turing smooth the grammar up to the\n"+
         "                 given frequency\n"+
	 "-o <ofile>       Output file for constructed ngram\n"+
   	 "\n"
	 "-input_format <string>\n"+
         "                 format of input data (default sentence_per_line)\n"+
	 "                 may be sentence_per_file, ngram_per_line.\n"+
	 "-otype <string>  format of output file, one of cstr_ascii\n"+
         "                 cstr_bin or htk_ascii\n"+
	 "-sparse          build ngram in sparse representation\n"+
         "-dense           build ngram in dense representation (default)\n"+
         "-backoff <int>\n"+
         "                 build backoff ngram (requires -smooth)\n"+
         "-floor <double>\n"+
         "                 frequency floor value used with some ngrams\n"+
	 "-freqsmooth <int>\n"+
         "                 build frequency backed off smoothed ngram, this\n"+
         "                 requires -smooth option\n"+
         "-trace           give verbose outout about build process\n"+
         "-save_compressed save ngram in gzipped format\n"+
         "-oov_mode <string>\n"+
         "                 what to do about out-of-vocabulary words,\n"+
         "                 one of skip_ngram, skip_sentence (default),\n"+
         "                 skip_file, or use_oov_marker\n"+
         "-oov_marker <string>\n"+
         "                 special word for oov words (default "+OOV_MARKER+")\n"+
         "                 (use in conjunction with '-oov_mode use_oov_marker'\n"+
         "\n"+
	 "Pseudo-words :\n"+
	 "-prev_tag <string>\n"+
         "                 tag before sentence start\n"+
	 "-prev_prev_tag <string>\n"+
         "                 all words before 'prev_tag'\n"+
	 "-last_tag <string>\n"+
         "                 after sentence end\n"+
	 "-default_tags    use default tags of "+SENTENCE_START_MARKER+
			","+SENTENCE_END_MARKER+" and "+SENTENCE_END_MARKER+"\n"+
         "                 respectively\n",
			files, al);

    if (al.present("-input_format"))
	input_format = al.val("-input_format");
    else
	input_format = "sentence_per_line";

    if (al.present("-oov_mode"))
	oov_mode = al.val("-oov_mode");
    else
	oov_mode = "skip_sentence";


    if(al.present("-oov_marker"))
    {
	if(oov_mode != "use_oov_marker")
	{
	    cerr << "Error : can only use -oov_marker with '-oov_mode use_oov_marker'" << endl;
	    exit(1);
	}
	else
	    oov_marker = al.val("-oov_marker");

	// should check oov marker is/isn't (?) in vocab
	// ......
    }

    if( (oov_mode != "skip_ngram") &&
       (oov_mode != "skip_sentence") &&
       (oov_mode != "skip_file") &&
       (oov_mode != "use_oov_marker") )
    {
	cerr << oov_mode << " is not a valid oov_mode !" << endl;
	exit(1);
    }

    if (al.present("-w"))
	wordlist_file = al.val("-w");
    else{
	cerr << "build_ngram: Must specify a wordlist with -w" << endl;
	exit(1);
    }

    if (load_StrList(wordlist_file,wordlist) != format_ok)
    {
	cerr << "build_ngram: Could not read wordlist from file " 
	    << wordlist_file << endl;
	exit(1);
    }


    if (al.present("-p"))
    {

	if(input_format != "ngram_per_line")
	{
	    cerr << "Can't have differering predictor/predictee lists unless data is in ngram_per_line format !" << endl;
	    exit(1);
	}

	wordlist_file2 = al.val("-p");
	if (load_StrList(wordlist_file2,wordlist2) != format_ok)
	{
	    cerr << "build_ngram: Could not read predictee list from file " 
		<< wordlist_file2 << endl;
	    exit(1);
	}
    }

    if (al.present("-trace"))
	trace=true;

    if (al.present("-o"))
	out_file = al.val("-o");
    else
	out_file = "-";
    
    if (al.present("-default_tags"))
    {	
	prev_tag = SENTENCE_START_MARKER;
	prev_prev_tag = SENTENCE_END_MARKER;
	last_tag = SENTENCE_END_MARKER;

	wordlist.append(SENTENCE_START_MARKER);
	wordlist.append(SENTENCE_END_MARKER);

	if (al.present("-p"))
	{
	    wordlist2.append(SENTENCE_START_MARKER);
	    wordlist2.append(SENTENCE_END_MARKER);
	}
    }
    
    if (al.present("-prev_tag"))
    {
	if (al.present("-default_tags"))
	    cerr << "build_ngram: WARNING : -prev_tag overrides -default_tags"
		<< endl;
	prev_tag = al.val("-prev_tag");
    }

    if (al.present("-prev_prev_tag"))
    {
	if (al.present("-default_tags"))
	    cerr << "build_ngram: WARNING : -prev_prev_tag overrides -default_tags" 
		<< endl;
	prev_prev_tag = al.val("-prev_prev_tag");
    }	
    
    if (al.present("-last_tag"))
    {
	if (al.present("-default_tags"))
	    cerr << "build_ngram: WARNING : -last_tag overrides -default_tags"
		<< endl;
	last_tag = al.val("-last_tag");
    }

    if (   ( (prev_tag=="") ||  (prev_prev_tag=="") || (last_tag=="") )
	&& ( (prev_tag!="") ||  (prev_prev_tag!="") || (last_tag!="") )   )
    {
	cerr << "build_ngram: ERROR : if any tags are given, ALL must be given"
	    << endl;
	exit(1);
    }

    if (al.present("-order"))
	order = al.ival("-order");
    else
    {
	cerr << "build_ngram: WARNING : No order specified with -order : defaulting to bigram" 
	    << endl;
	order = 2;
    }

    if (al.present("-otype"))
	format = al.val("-otype");
    else
	format = "";

    if (al.present("-floor"))
	floor = al.dval("-floor");
    else
	floor = 0.0;

    if (al.present("-backoff"))
	if (!al.present("-smooth"))
	{
	    cerr << "build_ngram: backoff requires smooth value" << endl;
	    exit(-1);
	}
    if (al.present("-freqsmooth"))
	if (!al.present("-smooth"))
	{
	    cerr << "build_ngram: frequency smooth requires smooth value"
		<< endl;
	    exit(-1);
	}

    if (al.present("-dense"))
	representation = EST_Ngrammar::dense;
    else if (al.present("-sparse"))
    {
	cerr << "build_ngram: Sorry, sparse representation is not yet available " << endl;
	exit(1);
	representation = EST_Ngrammar::sparse;
    }
    else if (al.present("-backoff"))
	representation = EST_Ngrammar::backoff;
    else
	cerr << "build_ngram: Defaulting to dense representation" << endl;
    
    if (al.present("-p"))
    {
	if (!ngrammar.init(order,representation,wordlist,wordlist2))
	{
	    cerr << "build_ngram: Failed to initialise " << order << "-gram" << endl;
	    exit(1);
	}
    }
    else
    {
	if (!ngrammar.init(order,representation,wordlist))
	{
	    cerr << "build_ngram: Failed to initialise " << order << "-gram" << endl;
	    exit(1);
	}
    }

    
    if ( al.present("-backoff") )
    {
	if (!ngrammar.build(files,prev_tag,prev_prev_tag,
			    last_tag,input_format,oov_mode,
			    al.ival("-backoff"),al.ival("-smooth")))
	{	      
	    cerr << "build_ngram: Failed to build backoff " << order 
		<< "-gram" << endl;
	    exit(1);
	}
	else if (trace)
	    cerr << "build_ngram: Built backoff " << order << 
		"-gram" << endl;
    }
    else
    {
	if (!ngrammar.build(files,prev_tag,prev_prev_tag,
			    last_tag,input_format,oov_mode))
	{
	    cerr << "build_ngram: Failed to build " << order << "-gram" << endl;
	    exit(1);
	}
	else
	    if(trace)
		cerr << "build_ngram: Built " << order << "-gram" << endl;
    }

    
    // Posit processing functions
    if (al.present("-freqsmooth"))
    {
	Ngram_freqsmooth(ngrammar,al.ival("-smooth"),al.ival("-freqsmooth"));
    }
    else if (al.present("-smooth") && !al.present("-backoff"))
    {
	int smoothcount = atoi(al.val("-smooth"));
	if(!Good_Turing_smooth(ngrammar,smoothcount,0))
	{
	    cerr << "build_ngram: Failed to smooth " << order << "-gram" << endl;
	    exit(1);
	}
	else
	    if(trace)
		cerr << "build_ngram: Good Turing smoothed " << order << "-gram" << endl;
	
    }
    
    // save
    if (al.present("-save_compressed"))
    {
	EST_String tmp_file = make_tmp_filename();
	if (ngrammar.save(tmp_file,format,trace,floor) == write_ok)
	{
	    EST_String prog_name;
	    EST_Pathname tmp(out_file);
	    if (tmp.extension() == GZIP_FILENAME_EXTENSION)
		prog_name = "gzip --stdout";
	    else if (tmp.extension() == COMPRESS_FILENAME_EXTENSION)
		prog_name = "compress -c";
	    else		// default
	    {
		prog_name = "gzip --stdout";
		if(out_file != "-")
		    out_file = out_file + "." + GZIP_FILENAME_EXTENSION;
	    }
	    
	    if (trace)
		cerr << "build_ngram: Compressing with '" << prog_name << "'" << endl;
	    
	    // now compress
	    if(compress_file(tmp_file,out_file,prog_name) != 0)
	    {
		cerr << "build_ngram: Failed to compress to file "
		    << out_file << endl;
		(void)delete_file(tmp_file);
		exit(1);
	    }
	    
	    (void)delete_file(tmp_file);
	    
	    if(trace)
		cerr << "build_ngram: Saved in compressed " << format
		    << " format to " << out_file << endl;
	}
	else
	{
	    cerr << "build_ngram: Failed to write temporary file " 
		<< tmp_file << endl;
	    exit(1);
	}
	
	
    }
    else
    {
	if (ngrammar.save(out_file,format,trace,floor) == write_ok)
	{
	    if(trace)
		cerr << "build_ngram: Saved in " << format
		    << " format to " << out_file << endl;
	}
	else
	{
	    cerr << "build_ngram: Failed to save " << format << " format data to " 
		<< out_file << endl;
	    exit(1);
	}
    }
    
    
    // everything went okay
    return 0;
}
