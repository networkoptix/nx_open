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
/*                     Date   :  September 1999                          */
/*-----------------------------------------------------------------------*/
/*  Simple alignment scoring program to give number of insertions,       */
/*  deletions, errors between a string of symbols and a reference string */
/*  of symbols                                                           */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstring>
#include "EST.h"
#include "EST_WFST.h"

static int align_main(int argc, char **argv);
static void nisttool_align(EST_Option &al);
static void string_align(EST_Option &al);
static void align_score(EST_Utterance &u, const EST_String &refrel,
			const EST_String &hyporel, 
			const EST_String &alignrel,
			int &total,int &ins,int &del,int &sub,int &correct);
static int name_distance(EST_Item *r,EST_Item *h);
void align(EST_Utterance &utt, 
	   const EST_String &refrel,
	   const EST_String &hyporel,
	   const EST_String &alignrel);
static void load_sentence(EST_Utterance &u, const EST_String &relname,
			  EST_TokenStream &ts);
static void load_sentence(EST_Utterance &u, const EST_String &relname,
			  EST_String &relval);

/** @name <command>align</command> <emphasis>align stream with reference stream</emphasis>
    @id align-manual
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

    align_main(argc,argv);

    exit(0);
    return 0;
}

static int align_main(int argc, char **argv)
{
    // Top level function generates a WFST from rules
    EST_Option al;
    EST_StrList files;
    EST_String outfile;
    EST_String format;

    parse_command_line
	(argc, argv,
	 EST_String("[options] ...\n")+
	 "Summary: align an hypothesis with a reference string\n"+
	 "-rfile <ifile>    Reference file\n"+
	 "-hfile <ifile>    Hypothesis file\n"+
	 "-rstring <string> Reference string\n"+
	 "-hstring <string> Hypothesis string\n"+
	 "-format <string>\n"+
         "               Supported formats: strings, nisttool\n",
		       files, al);
    
    if (al.present("-o"))
	outfile = al.val("-o");
    else
	outfile = "-";

    if (al.present("-format"))
	format = al.val("-format");
    else
	format = "strings";

    if (format == "strings")
        string_align(al);
    else if (format == "nisttool")
	nisttool_align(al);
    else
        cout << "Unknown or unhandled format: " << format << endl;	

    return 0;
}

bool dp_match(const EST_Relation &lexical,
	      const EST_Relation &surface,
	      EST_Relation &match,
	      float ins, float del, float sub);

static void string_align(EST_Option &al)
{
    EST_String refStr = al.val("-rstring");
    EST_String hypStr = al.val("-hstring");
    EST_Utterance u;
    int total,ins,del,sub,correct;

    load_sentence(u,"ref",refStr);
    load_sentence(u,"hypo",hypStr);
    align(u,"ref","hypo","align");
    align_score(u,"ref","hypo","align",total,ins,del,sub,correct);
    fprintf(stdout,"words %d\n",total);
    fprintf(stdout,"insertions %d\n",ins);
    fprintf(stdout,"deletions %d\n",del);
    fprintf(stdout,"substitutions %d\n",sub);
    fprintf(stdout,"correct %d\n",correct);
    fprintf(stdout,"WER %f\n",(100.0 * (float)(ins+del+sub))/total);
}

static void nisttool_align(EST_Option &al)
{
    // Using the format used by the NIST tools for alignment
    // Sentence per line with parenthesized id at end
    EST_String reffile = al.val("-rfile");
    EST_String hypofile = al.val("-hfile");
    EST_TokenStream rts,hts;
    EST_Item *r, *h;
    static EST_Regex id("^(.*)$");
    int sents=0;
    int total,ins,del,sub,correct;
    int s_total,s_ins,s_del,s_sub,s_correct;

    rts.open(reffile);
    hts.open(hypofile);
    s_total=s_ins=s_del=s_sub=s_correct=0;

    while (!rts.eof())
    {
	EST_Utterance u;
	
	load_sentence(u,"ref",rts);
	load_sentence(u,"hypo",hts);
	r = u.relation("ref")->last();
	h = u.relation("hypo")->last();
	if ((!r->name().matches(id)) ||
	    (r->name() != h->name()))
	{
	    cerr << "Align: failed to match sentence " <<
		sents << " at id " << r->name() << endl;
	}
	else
	{
	    // Ids aren't counted as words
	    r->unref_all();
	    h->unref_all();
	    align(u,"ref","hypo","align");
	    // This doesn't give exactly the same as the NIST tools
	    // even though it should (actually I think its better)
//	    dp_match(*u.relation("ref"),
//		     *u.relation("hypo"),
//		     *u.create_relation("align"),
//		     3,3,4);
	    align_score(u,"ref","hypo","align",
			total,ins,del,sub,correct);
	    s_total += total;
	    s_ins += ins;
	    s_del += del;
	    s_sub += sub;
	    s_correct += correct;
	}
        sents++;
    }

    rts.close();
    hts.close();
    fprintf(stdout,"sentences %d\n",sents);
    fprintf(stdout,"words %d\n",s_total);
    fprintf(stdout,"insertions %d\n",s_ins);
    fprintf(stdout,"deletions %d\n",s_del);
    fprintf(stdout,"substitutions %d\n",s_sub);
    fprintf(stdout,"correct %d\n",s_correct);
    fprintf(stdout,"WER %f\n",(100.0 * (float)(s_ins+s_del+s_sub))/s_total);
}

static void load_sentence(EST_Utterance &u,
			  const EST_String &relname,
			  EST_TokenStream &ts)
{
    EST_Relation *r = u.create_relation(relname);

    do
    {
	EST_Item *i = r->append();
	i->set_name(ts.get());
    }
    while ((!ts.eoln()) && (!ts.eof()));
}

static void load_sentence(EST_Utterance &u,
			  const EST_String &relname,
			  EST_String &relval)
{
    EST_Relation *r = u.create_relation(relname);
    EST_StrList strlist;
    StringtoStrList(relval, strlist, " ");
    EST_StrList::Entries iter;

    for (iter.begin(strlist); iter; ++iter)
    {
        EST_Item *i = r->append();
	i->set_name(*iter);
    }
}

static void align_score(EST_Utterance &u, const EST_String &refrel,
			const EST_String &hyporel, 
			const EST_String &alignrel,
			int &total,int &ins,int &del,int &sub,int &correct)
{
    // Score alignment
    EST_Item *ri,*hi;
    total=ins=del=correct=sub=0;

    for (ri=u.relation(refrel)->first(),
	 hi=u.relation(hyporel)->first();
	 ri;
	 ri=ri->next(),hi=hi->next())
    {
	for ( ; (as(hi,alignrel) == 0) && hi ; hi=hi->next())
	{
	    fprintf(stdout,"inserted: %s\n",(const char *)hi->name());
	    ins++;
	}
	for ( ; (daughter1(ri,alignrel) == 0) && ri; ri=ri->next())
	{
	    fprintf(stdout,"deleted: %s\n",(const char *)ri->name());
	    del++;
	}
	if (!ri)
	    break;
	if (name_distance(ri,daughter1(ri,alignrel)) == 0)
	{
	    fprintf(stdout,"correct: %s\n",(const char *)ri->name());
	    correct++;
	}
	else
	{
	    fprintf(stdout,"substituted: %s\n",(const char *)ri->name());
	    sub++;
	}
    }
    // For trailing hypothesized (or ref is nil)
    for ( ;  hi ; hi=hi->next())
    {
	fprintf(stdout,"inserted: %s\n",(const char *)hi->name());
	ins++;
    }

    total = u.relation(refrel)->length();


//    fprintf(stdout,"total %d ins %d del %d subs %d correct %d\n",
//	    total, ins, del, sub, correct);
}

static int name_distance(EST_Item *r,EST_Item *h)
{
    EST_String rname = r->name();
    EST_String hname = h->name();
    if ((rname == hname) ||
	(downcase(rname) == downcase(hname)))
	return 0;
    else
	return 1;
}

void align(EST_Utterance &utt,
	   const EST_String &refrel,
	   const EST_String &hyporel,
	   const EST_String &alignrel)
{
    // Align refrel to hyporel by alignrel
    int r_size = utt.relation(refrel)->length();
    int h_size = utt.relation(hyporel)->length();
    EST_Item *ri = utt.relation(refrel)->first();
    EST_Item *hi = utt.relation(hyporel)->first();
    int i,j;
    int insdel_cost = 3;
    int subs_cost = 4;
    float to_insert,to_del,to_subs;
    float cost;

    EST_Relation *ar = utt.create_relation(alignrel);

    EST_FMatrix dpt(r_size+1,h_size+1);
    EST_IMatrix dpp(r_size+1,h_size+1);

    // Initialise first row and column
    dpt(0,0) = subs_cost * name_distance(ri,hi);
    dpp(0,0) = 0;
    for (i=1; i<r_size+1; i++)
    {
	dpt(i,0) = insdel_cost + dpt(i-1,0);
	dpp(i,0) = -1;  // deletion
    }
    for (j=1; j < h_size+1; j++)
    {
	dpt(0,j) = insdel_cost + dpt(0,j-1);
	dpp(0,j) = 1;   // insertion
    }

    ri = utt.relation(refrel)->first();
    for (i=1; ri; ri=ri->next(),i++)
    {
	ar->append(ri);  // for use later
	hi = utt.relation(hyporel)->first();
	for (j=1; hi; hi=hi->next(),j++)
	{
	    cost = name_distance(ri,hi);
	    to_insert = insdel_cost + dpt(i,j-1);
	    to_del = insdel_cost + dpt(i-1,j);
	    to_subs = (cost * subs_cost) + dpt(i-1,j-1);
	    if (to_insert < to_del)
	    {
		if (to_insert < to_subs)
		{
		    dpt(i,j) = to_insert;
		    dpp(i,j) = 1;
		}
	        else
		{
		    dpt(i,j) = to_subs;
		    dpp(i,j) = 0;
		}
	    }
	    else
	    {
		if (to_del < to_subs)
		{
		    dpt(i,j) = to_del;
		    dpp(i,j) = -1;
		}
	        else
		{
		    dpt(i,j) = to_subs;
		    dpp(i,j) = 0;
		}
	    }
	}
    }

//      for (i=1,ri=utt.relation(refrel)->first(); i < r_size+1; i++,ri=ri->next())
//      {
//  	fprintf(stdout,"%10s  ",(const char *)ri->name());
//  	for (j=1,hi=utt.relation(hyporel)->first(); j<h_size+1; j++,hi=hi->next())
//  	    fprintf(stdout,"%3d/%2d ",(int)dpt(i,j),dpp(i,j));
//  	fprintf(stdout,"\n");
//      }

    for (i=r_size,j=h_size,
	     ri=utt.relation(refrel)->last(),
	     hi=utt.relation(hyporel)->last();
	 ri; i--,ri=ri->prev())
    {
	while (dpp(i,j) == 1)
	{
	    j--;
//	    fprintf(stdout,"skipping hi %s\n",
//		    (const char *)hi->name());
	    hi=hi->prev();
	}
	if (dpp(i,j) == 0)
	{
//	    fprintf(stdout,"linking %s %s\n",
//		    (const char *)ri->name(),
//		    (const char *)hi->name());
	    append_daughter(ri,alignrel,hi);
	    j--;
	    hi=hi->prev();
	}
//	else
//	    fprintf(stdout,"skipping ri %s\n",
//		    (const char *)ri->name());
    }
}
