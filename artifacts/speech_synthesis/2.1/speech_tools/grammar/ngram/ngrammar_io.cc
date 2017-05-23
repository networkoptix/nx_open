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
/*                     Author :  Simon King & Alan W Black               */
/*                     Date   :  February 1997                           */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* IO functions for EST_Ngram class                                      */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <fstream>
#include <iostream>
#include "EST_unix.h"
#include <cstring>
#include <climits>
#include <cfloat>
#include "EST_String.h"
#include "EST_Ngrammar.h"
#include "EST_Token.h"
#include "EST_cutils.h"

EST_read_status
load_ngram_htk_ascii(const EST_String filename, EST_Ngrammar &n)
{
    (void)filename;
    (void)n;
    return wrong_format;
}

EST_read_status
load_ngram_htk_binary(const EST_String filename, EST_Ngrammar &n)
{
    (void)filename;
    (void)n;
    return wrong_format;
}

EST_read_status
load_ngram_arpa(const EST_String filename, EST_Ngrammar &n, const EST_StrList &vocab)
{

    EST_TokenStream ts;
    EST_String s;
    int i,j,k, order=0;
    double occur,weight;
    int this_num,this_order;

    if (ts.open(filename) == -1)
	return misc_read_error;

    // find  backslash data backslash
    while ((!ts.eof()) && !ts.get().string().contains("\\data\\"));

    if (ts.eof())
    {
	ts.close();
	return wrong_format;
    }

    // find order and numbers of ngrams

    // somewhere to keep numbers
    EST_IVector nums(100); // not going to have anything bigger than a 100-gram !

    while (!ts.eof())
    {
	// have we got to next section
	if (ts.peek().string().contains("-grams:"))
	    break;

	s=ts.get_upto_eoln().string();

	if(s.contains("ngram ") && s.contains("="))
	{

	    s=s.after("ngram ");
	    this_order=atoi(s.before("="));
	    this_num=atoi(s.after("="));

	    //cerr << "There are " << this_num << " " << this_order
	    //<< "-grams" << endl;

	    nums[this_order] = this_num;

	    if(this_order > order)
		order = this_order;
	}

    }


    if(order==0)
    {
	//cerr << "No ngram ?=? in header !" << endl;
	ts.close();
	return wrong_format;
    }

    //cerr << "Initialising " << order << "-grammar" << endl;
    if(!n.init(order,EST_Ngrammar::backoff,vocab))
	return misc_read_error;

    // read data
    for(i=1;i<=order;i++)
    {

	EST_StrVector window(i);

	// find start of data for this order "<order>-grams:"
	EST_String tmp =  "\\" + itoString(i) + "-grams:";
	while (!ts.eof())
	{
	    s=ts.get().string();
	    if (s.contains(tmp))
		break;
	}


	if(ts.eof())
	{
	    cerr << "Unexpected end of grammar file whilst looking for '"
		<< tmp << "'" << endl;
	    return misc_read_error;
	}
	
	//cerr << "Found order " << i << " : " << tmp << endl;
	//cerr << "Looking for " << nums(i) << " ngrams" << endl;
	// look for nums(i) ngrams

	for(j=0;j<nums(i);j++)
	{
	    
	    for (k=0; ((k<i) && !ts.eof()); k++)
		window[k] = ts.get().string();

	    if(ts.eof())
	    {
		cerr << "Unexpected end of file whilst reading " << i
		    << "-grams !" << endl;
		return misc_read_error;
	    }

	    occur = atof(ts.get().string());


	    // can't for backoff grammars, need to set probs directly
	    
	    cerr << "ooooooooops" << endl;
	    return wrong_format;
	    //n.accumulate(window,occur);

	    // backoff weight ?
	    if (!ts.eoln())
	    {
		weight = atof(ts.get().string());
		n.set_backoff_weight(window,weight);
	    }
	    
	    if (!ts.eoln())
	    {
		cerr << "EST_Ngrammar:load_ngram_arpa expect end of line at filepos "
		    << ts.filepos() << endl;
		ts.close();
		return misc_read_error;
	    }
	}
	
    } // loop through orders
    

    // find backslash end backslash 
    while (!ts.eof())
	if (ts.get().string() == "\\end\\")
	{
	    ts.close();
	    return format_ok;

	}

    cerr << "Missing \\end\\ !" << endl;

    ts.close();
    return misc_read_error;

}

EST_read_status
load_ngram_cstr_ascii(const EST_String filename, EST_Ngrammar &n)
{
    EST_TokenStream ts;
    int i, order;
    double occur;
    
    if (ts.open(filename) == -1)
	return misc_read_error;

    if (ts.peek().string() != "Ngram_2")
    {
	ts.close();
	return wrong_format;
    }
    ts.get();			// skip magic number
    
    order = atoi(ts.get().string());
    ts.get_upto_eoln();		// skip to next line
    EST_StrList vocab;
    EST_StrList pred_vocab;	// may be different
    
    while (!ts.eoln())
	vocab.append(ts.get().string());
    ts.get_upto_eoln();		// skip to next line
    while (!ts.eoln())
	pred_vocab.append(ts.get().string());
    
    if(!n.init(order,EST_Ngrammar::dense,vocab,pred_vocab))
    {
	cerr << "Something may be wrong with the vocab lists in '"
	    << filename << "'" << endl;
	return misc_read_error;
    }
    
    EST_StrVector window(order);
    
    while(!ts.eof())
    {
	for (i=0; i < order; i++)
	    window[i] = ts.get().string();
	if (ts.get().string() != ":")
	{
	    cerr << "EST_Ngrammar:load_ngram_cstr_ascii missing colon at filepos "
		<< ts.filepos() << endl;
	    return misc_read_error;
	}
	occur = atof(ts.get().string());
	n.accumulate(window,occur);
	if (!ts.eoln())
	{
	    cerr << "EST_Ngrammar:load_ngram_cstr_ascii expect end of line at filepos "
		<< ts.filepos() << endl;
	    return misc_read_error;
	}
    }
    
    ts.close();
    
    return format_ok;
}

EST_read_status 
load_ngram_cstr_bin(const EST_String filename, EST_Ngrammar &n)
{
    EST_TokenStream ts;
    int i,j,order;
    EST_Litem *k;
    int num_entries;
    double approx_num_samples = 0.0;
    long freq_data_start, freq_data_end;
    FILE *ifd;
    int magic = 0;
    int swap = FALSE;
    
    if ((ifd=fopen(filename,"rb")) == NULL)
	return misc_read_error;
    fread(&magic,sizeof(int),1,ifd);
    
    if (SWAPINT(magic) == EST_NGRAMBIN_MAGIC)
	swap = TRUE;
    else if (magic != EST_NGRAMBIN_MAGIC)
	return wrong_format;
    if (ts.open(ifd, FALSE) == -1)
	return misc_read_error;
    
    ts.set_SingleCharSymbols("\n");
    ts.set_WhiteSpaceChars(" \t\r");
    
    if (ts.peek().string() != "mBin_2")
    {
	fclose(ifd);
	ts.close();
	return wrong_format;
    }
    ts.get();			// skip magic number
    
    order = atoi(ts.get().string());
    if (ts.get() != "\n")
    {
	fclose(ifd);
	ts.close();
	return misc_read_error;
    }
    EST_StrList vocab;
    EST_StrList pred_vocab;	// may be different
    
    while ((ts.peek() != "\n") && (!ts.eof()))
	vocab.append(ts.get().string());
    ts.get();			// skip newline
    while ((ts.peek() != "\n") && (!ts.eof()))
	pred_vocab.append(ts.get().string());
    
    // Need to get to the position one after the newline and
    // who knows what TokenStream has already read,
    fseek(ifd,(long)(ts.peek().filepos()+5),SEEK_SET);
    
    if(!n.init(order,EST_Ngrammar::dense,vocab,pred_vocab))
    {
	ts.close();
	fclose(ifd);
	return misc_read_error;
    }
    
    EST_StrVector window(order);
    
    freq_data_start = ftell(ifd);
    fseek(ifd,0,SEEK_END);
    freq_data_end = ftell(ifd);
    num_entries = (freq_data_end-freq_data_start)/sizeof(double);
    double *dd = new double[num_entries];
    
    // Go back to start of data
    fseek(ifd,freq_data_start,SEEK_SET);
    
    if (fread(dd,sizeof(double),num_entries,ifd) != (unsigned)num_entries)
    {
	cerr << "EST_Ngrammar::load_ngram_cstr_bin format does not have expected number of entries" << endl;
	ts.close();
	fclose(ifd);
	return misc_read_error;
    }
    if (swap)
	swap_bytes_double(dd,num_entries);
    
    for(j=i=0;i<n.num_states();i++)
    {
	if (j >= num_entries)
	{
	    cerr << "EST_Ngrammar::load_ngram_cstr_bin unexpected end of frequency data" << endl;
	    ts.close();
	    fclose(ifd);
	    return misc_read_error;	
	}
	for (k=n.p_states[i].pdf().item_start();
	     (!n.p_states[i].pdf().item_end(k)) && (j < num_entries) ;
	     k = n.p_states[i].pdf().item_next(k))
	{
	    n.p_states[i].pdf().set_frequency(k,dd[j]);
	    // Update global info too
	    approx_num_samples += dd[j]; // probably not right
	    n.vocab_pdf.cumulate(k,dd[j]);
	    
	    // Number of consecutive occurrences of this frequency as in
	    // dd[j+1] if its a negative number
	    if (j+1 >= num_entries)
		j++;
	    else if (dd[j+1] < -1)
		dd[j+1]++;
	    else if (dd[j+1] == -1)
		j +=2;
	    else
		j++;
	}
    }
    
    // With smoothing num_samples might not be as exact as you like
    n.p_num_samples = (int)approx_num_samples;
    
    delete [] dd;
    
    ts.close();
    fclose(ifd);
    
    return format_ok;
}

// ====================================================================

EST_write_status
save_ngram_htk_ascii_sub(const EST_String &word, ostream *ost, 
			 EST_Ngrammar &n, double floor)
{
    EST_Litem *k;
    EST_String name;
    double freq;
    EST_StrVector this_ngram(2); // assumes bigram
    this_ngram[0] = word;
    EST_DiscreteProbDistribution this_pdf;
    this_pdf = n.prob_dist(this_ngram);
    
    double lfreq=-1;
    int lcount=0;
    double total_freq=0;
    
    double floor_prob_total = floor * (n.pred_vocab->length()-1);
    
    if (word == n.p_sentence_end_marker)
    {
	*ost << word;
	*ost << " 0*" << n.pred_vocab->length()-1 << " " << 1 << endl;
	return write_ok;
    }
    
    if(floor_prob_total > 1)
    {
	cerr << "ERROR : floor is impossibly large, scaling it !" << endl;
	floor = 1.0 / (double)(n.pred_vocab->length()-1);
	floor_prob_total = 1;
    }
    
    // not efficient but who cares ?
    for (k=this_pdf.item_start();
	 !this_pdf.item_end(k);
	 k = this_pdf.item_next(k))
    {
	this_pdf.item_freq(k,name,freq);
	if(name != n.p_sentence_start_marker)
	{
	    total_freq += freq;
	}
    }
    
    
    // 0 for prob(word,start marker)
    *ost << word << " 0 ";
    
    if (total_freq <= 0)
    {
	*ost << 1.0 / (double)(n.pred_vocab->length()-1) << "*";
	*ost << n.pred_vocab->length()-1 << " " << endl;
    }
    else
    {
	lfreq=-1;
	
	for (k=this_pdf.item_start();
	     !this_pdf.item_end(k);
	     k = this_pdf.item_next(k))
	{
	    this_pdf.item_freq(k,name,freq);
	    
	    if ( (name == n.p_sentence_start_marker) ||
		(name == n.p_sentence_end_marker) ||
		(name == OOV_MARKER) )
		continue;
	    
	    if (freq == lfreq)
		lcount++;
	    else
	    {
		if (lcount > 1)
		    *ost << "*" << lcount << " ";
		else
		    *ost << " ";
		
		lcount=1;
		lfreq = freq;
		
		if(freq > 0)
		{
		    double base_prob = freq / total_freq;
		    
		    // and floor/scale it
		    *ost << floor + ( base_prob * (1-floor_prob_total) );
		    
		}
		else
		    *ost << floor;
		
	    }
	    
	    
	}
	
    }				// total_freq > 0
    
    
    if(!n.closed_vocab())
    {
	
	// not fully tested !!!!!!!!
	
	*ost << 0 << " ERROR !!!!!!!! ";
    }
    
    
    if (total_freq > 0)
    {
	freq = this_pdf.frequency(n.p_sentence_end_marker);
	
	if(freq == lfreq)
	{
	    lcount++;
	    *ost << "*" << lcount << " " << endl;
	}
	else
	{
	    
	    if (lcount > 1)
		*ost << "*" << lcount << " ";
	    else
		*ost << " ";
	    
	    if(freq > 0)
	    {
		double base_prob = freq / total_freq;
		
		// and floor/scale it
		*ost << floor + ( base_prob * (1-floor_prob_total) ) << endl;
		
	    }
	    else
		*ost << floor << endl;
	}
    }
    
    return write_ok;
}

EST_write_status
save_ngram_htk_ascii(const EST_String filename, 
		     EST_Ngrammar &n, double floor)
{
    
    ostream *ost;

    // only for bigram
    if(n.order() != 2)
    {
	cerr << "Can only save bigrams in htk_ascii format" << endl;
	return misc_write_error;
    }
    
    if (floor < 0)
    {
	cerr << "Negative floor probability does not make sense !" << endl;
	return misc_write_error;
    }
    
    if (filename == "-")
	ost = &cout;
    else
	ost = new ofstream(filename);
    
    if(!(*ost))
	return write_fail;
    
    if(floor * (n.pred_vocab->length()-1) > 1)
    {
	floor = 1.0 / (double)(n.pred_vocab->length()-1);
	cerr << "ERROR : floor is impossibly large, scaling it to ";
	cerr << floor << endl;
    }
    
    int i;
    
    if(n.p_sentence_start_marker == "")
    {
	cerr << "Can't save in HTK format as no sentence start/end tags"
	    << " were given !" << endl;
	return misc_write_error;
    }
    
    // need '!ENTER' (or whatever) as first word- that's HTK for you
    save_ngram_htk_ascii_sub(n.p_sentence_start_marker,ost,n,floor);
    
    // the real words
    for(i=0;i<n.vocab->length();i++)
    {
	if ( (n.vocab->name(i) != n.p_sentence_start_marker) &&
	    (n.vocab->name(i) != n.p_sentence_end_marker) &&
	    (n.vocab->name(i) != OOV_MARKER) )
	    save_ngram_htk_ascii_sub(n.vocab->name(i),ost,n,floor);
    }
    
    if(!n.closed_vocab())
	save_ngram_htk_ascii_sub(OOV_MARKER,ost,n,floor);
    
    save_ngram_htk_ascii_sub(n.p_sentence_end_marker,ost,n,floor);
    
    if(ost != &cout)
	delete ost;
    
    return write_ok;
}

/*
   EST_write_status
   save_ngram_htk_binary(const EST_String filename, EST_Ngrammar &n)
   {
   return write_ok;
   }
   */

void
count_ngram_arpa_sub(EST_Ngrammar *n, EST_StrVector &ngram, void *count)
{
    if(n->ngram_exists(ngram))
	*((double*)count) += 1;
}

void
save_ngram_arpa_sub(EST_Ngrammar *n, EST_StrVector &ngram, void *ost)
{
    
    int i;
    
    if(n->ngram_exists(ngram))
    {
	*((ostream*)(ost)) << safe_log10(n->probability(ngram)) << " ";
	for(i=0;i<ngram.n();i++)
	    *((ostream*)(ost)) << ngram(i) << " ";
	
	if ((n->representation() == EST_Ngrammar::backoff) &&
	    (n->order() > ngram.n()) )
	    *((ostream*)(ost)) << safe_log10(n->get_backoff_weight(ngram));
	//<< " = "
	//<< n->get_backoff_weight(ngram) << " ";
	
	*((ostream*)(ost)) << endl;
	
    }
}

EST_write_status
save_ngram_arpa(const EST_String filename, EST_Ngrammar &n)
{
    // ARPA MIT-LL format - see HTK manual !!
    
    ostream *ost;
    int i,num_n,o;
    
    if (filename == "-")
	ost = &cout;
    else
	ost = new ofstream(filename);
    
    if (!(*ost))
	return write_fail;
    
    //n.set_entry_type(EST_Ngrammar::probabilities);
    //n.make_htk_compatible(); // fix enter/exit probs
    //*ost << *(n.vocab) << endl;
    
    // count number of ngrams
    num_n = (int)n.samples();
    *ost << "\\data\\" << endl;
    
    double *count = new double;
    
    if (n.representation() == EST_Ngrammar::backoff)
    {
	for(o=1;o<=n.order();o++)
	{
	    EST_StrVector ngram(o);
	    for(i=0;i<o;i++)
		ngram[i] = "";
	    *count =0;
	    
	    // this is a deeply silly way to count them,
	    // we could traverse the tree directly !
	    n.iterate(ngram,&count_ngram_arpa_sub,(void*)count);
	    *ost << "ngram " << o << "=" << *count << endl;
	}
	
	for(o=1;o<=n.order();o++)
	{
	    *ost << endl;
	    *ost << "\\" << o << "-grams:" << endl;
	    EST_StrVector ngram(o);
	    for(i=0;i<o;i++)
		ngram[i] = "";
	    n.iterate(ngram,&save_ngram_arpa_sub,(void*)ost);
	}
	
    }
    else
    {
	EST_StrVector ngram(n.order());
	for(i=0;i<n.order();i++)
	    ngram[i] = "";
	*count =0;
	n.iterate(ngram,&count_ngram_arpa_sub,(void*)count);
	*ost << "ngram " << n.order() << "=" << *count << endl;
	
	*ost << endl;
	*ost << "\\" << n.order() << "-grams:" << endl;
	
	for(i=0;i<n.order();i++)
	    ngram[i] = "";
	n.iterate(ngram,&save_ngram_arpa_sub,ost);
	
    }
    
    *ost << "\\end\\" << endl;
    
    if (ost != &cout)
	delete ost;
    
    return write_ok;
}

EST_write_status 
save_ngram_cstr_ascii(const EST_String filename, EST_Ngrammar &n,
		      const bool trace, double floor)
{
    // awb's format
    (void)trace;
    ostream *ost;
    int i;
    EST_Litem *k;
    
    if (filename == "-")
	ost = &cout;
    else
	ost = new ofstream(filename);
    
    if(!(*ost))
	return write_fail;
    
    *ost << "Ngram_2 " << n.order() << endl;
    for (i=0; i < n.vocab->length(); i++)
	*ost << n.vocab->name(i) << " ";
    *ost << endl;
    for (i=0; i < n.pred_vocab->length(); i++)
	*ost << n.pred_vocab->name(i) << " ";
    *ost << endl;
    
    if (n.representation() == EST_Ngrammar::dense)
	n.print_freqs(*ost,floor);
    else if (n.representation() == EST_Ngrammar::backoff)
    {
      int total_ngrams = (int)pow(float(n.get_vocab_length()),float(n.order()-1));
	
	for(i=0;i<total_ngrams;i++)
	{
	    EST_DiscreteProbDistribution this_pdf;
	    const EST_StrVector this_ngram = n.make_ngram_from_index(i);
	    this_pdf = n.prob_dist(this_ngram);
	    
	    for (k=this_pdf.item_start();
		 !this_pdf.item_end(k);
		 k = this_pdf.item_next(k))
	    {
		double freq;
		EST_String name;
		this_pdf.item_freq(k,name,freq);
		
		for (int jj=0; jj < this_ngram.n(); jj++)
		    *ost << this_ngram(jj) << " ";
		*ost << name << " : " << freq << endl;
	    }
	}
    }
    
    if(ost != &cout)
	delete ost;
    
    return write_ok;
}

EST_write_status 
save_ngram_wfst(const EST_String filename, EST_Ngrammar &n)
{
    // Save as a WFST
    FILE *ost;
    int i;

    if ((ost = fopen(filename,"wb")) == NULL)
    {
	cerr << "Ngrammar save: unable to open \"" << filename << 
	    "\" for writing" << endl;
	return write_fail;
    }

    fprintf(ost,"EST_File fst\n");
    fprintf(ost,"DataType ascii\n");
    fprintf(ost,"in \"(");
    for (i=0; i < n.vocab->length(); i++)
	fprintf(ost," %s\n",(const char *)n.vocab->name(i));
    fprintf(ost," )\"\n");
    fprintf(ost,"out \"(");
    for (i=0; i < n.vocab->length(); i++)
	fprintf(ost," %s\n",(const char *)n.vocab->name(i));
    fprintf(ost," )\"\n");
    fprintf(ost,"NumStates %d\n",n.num_states());
    fprintf(ost,"EST_Header_End\n");

    for (i=0; i<n.num_states(); i++)
    {
	fprintf(ost,"((%d nonfinal %d)\n",i,i);
	fprintf(ost,")\n");
    }

    fclose(ost);
    
    return write_ok;
}

EST_write_status 
save_ngram_cstr_bin(const EST_String filename, EST_Ngrammar &n, 
		    const bool trace, double floor)
{
    
    if (n.representation() == EST_Ngrammar::sparse)
	return misc_write_error;
    
    int i;
    EST_Litem *k;
    FILE *ofd;
    double lfreq = -1;
    double count = -1;
    int magic = EST_NGRAMBIN_MAGIC;
    
    if (filename == "-")
    {
	if ((ofd=stdout) == NULL)
	    return misc_write_error;
    }
    else
    {
	if ((ofd=fopen(filename,"wb")) == NULL)
	    return misc_write_error;
    }
    
    fwrite(&magic,sizeof(int),1,ofd);
    fprintf(ofd,"mBin_2 %d\n",n.order());
    for (i=0; i < n.vocab->length(); i++)
	fprintf(ofd,"%s ",(const char *)n.vocab->name(i));
    fprintf(ofd,"\n");
    for (i=0; i < n.pred_vocab->length(); i++)
	fprintf(ofd,"%s ",(const char *)n.pred_vocab->name(i));
    fprintf(ofd,"\n");
    
    // We use a simple form of run-length encoding, if consecutive
    // values are equal only a length is printed.  lengths are
    // negative as frequencies (even smoothed ones) can never be -ve
    
    if ( trace )
	cerr << "Saving ..." << endl;
    
    if (n.representation() == EST_Ngrammar::dense)
    {
	for(i=0;i<n.num_states();i++)
	{
	    
	    if ( trace )
		cerr << "\r" << i*100/n.num_states() << "%";
	    
	    for (k=n.p_states[i].pdf().item_start();
		 !n.p_states[i].pdf().item_end(k);
		 k = n.p_states[i].pdf().item_next(k))
	    {
		double freq;
		EST_String name;
		n.p_states[i].pdf().item_freq(k,name,freq);
		if (freq == 0.0)
		    freq = floor;
		if (freq == lfreq)
		    count--;
		else
		{
		    if (count < -1)
			fwrite(&count,sizeof(double),1,ofd);
		    fwrite(&freq,sizeof(double),1,ofd);
		    count = -1;
		}
		lfreq = freq;
	    }
	}
	if (count < -1)
	    fwrite(&count,sizeof(double),1,ofd);
    }
    else if (n.representation() == EST_Ngrammar::backoff)
    {
	// need to construct pdfs in right order
	// noting that dense states are indexed s.t. the last
	// word in the ngram is the least significant 'bit'
	
	// number of ngrams, excluding last word, is
      int total_ngrams = (int)pow(float(n.get_vocab_length()),float(n.order()-1));
	
	for(i=0;i<total_ngrams;i++)
	{
	    
	    if ( trace )
		cerr << "\r" << i*100/total_ngrams << "%";
	    
	    EST_DiscreteProbDistribution this_pdf;
	    const EST_StrVector this_ngram = n.make_ngram_from_index(i);
	    this_pdf = n.prob_dist(this_ngram);
	    
	    for (k=this_pdf.item_start();
		 !this_pdf.item_end(k);
		 k = this_pdf.item_next(k))
	    {
		
		double freq;
		EST_String name;
		this_pdf.item_freq(k,name,freq);
		if (freq == lfreq)
		    count--;
		else
		{
		    if (count < -1)
			fwrite(&count,sizeof(double),1,ofd);
		    fwrite(&freq,sizeof(double),1,ofd);
		    count = -1;
		}
		lfreq = freq;
	    }
	    
	    
	}
	
    }
    if ( trace )
	cerr << "\r      \r" << endl;
    
    fclose(ofd);
    
    return write_ok;
}
