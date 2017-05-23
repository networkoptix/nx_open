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
/*                      Author :  Paul Taylor updated by awb             */
/*                      Date   :  Feb 1999                               */
/*-----------------------------------------------------------------------*/
/*                      Relation class file i/o, label files             */
/*                                                                       */
/*=======================================================================*/
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include "EST_unix.h"
#include "EST_types.h"
#include "ling_class/EST_Relation.h"
#include "EST_string_aux.h"
#include "EST_cutils.h"
#include "EST_TList.h"
#include "EST_Option.h"
#include "relation_io.h"

#define DEF_SAMPLE_RATE 16000
#define HTK_UNITS_PER_SECOND 10000000

static EST_Regex RXleadingwhitespace("^[ \t\n\r][ \t\n\r]*.*$");

EST_read_status read_label_portion(EST_TokenStream &ts, EST_Relation &s, 
				   int sample);

EST_read_status load_esps_label(EST_TokenStream &ts,EST_Relation &rel)
{
    ts.set_SingleCharSymbols(";");
    ts.set_quotes('"','\\');
    EST_String key, val;

    // Skip the header
    while (!ts.eof())
    {
	key = ts.get().string();
        if (key == "#")
            break;

	val = ts.get_upto_eoln().string();
	// delete leading whitespace
	if (val.matches(RXleadingwhitespace))
	    val = val.after(RXwhite);
	rel.f.set(key, val);
    }
	    
    if (ts.peek() == "") return format_ok;

    while (!ts.eof())
    {
	EST_Item *si = rel.append();
	EST_String name;
	
	si->set("end",(float)atof(ts.get().string()));
	ts.get();  // skip the color;

	for (name = ""; (!ts.eoln()) && (ts.peek() != ";"); )
	{
	    EST_Token &t = ts.get();
	    if (name.length() > 0)	// preserve internal whitespace
		name += t.whitespace();  
	    name += t.string();
	}
	si->set_name(name);
	
	if (ts.peek().string() == ";") // absorb separator
	{
	    ts.get();
	    si->features().load(ts);
	}
    }
    return format_ok;
}

EST_write_status save_esps_label(const EST_String &filename, 
				 const EST_Relation &s,
				 bool evaluate_ff)
{
    ostream *outf;
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))  
    {
	cerr << "save_esps_label: can't open label output file \"" << 
	    filename << "\"" << endl;
	return write_fail;
    }

    EST_write_status st=save_esps_label(outf, s, evaluate_ff);
  
    if (outf != &cout)
	delete outf;

    return st;
}

EST_write_status save_esps_label(ostream *outf,
				 const EST_Relation &s,
				 bool evaluate_ff)
{
    EST_Item *ptr;
    
    *outf << "separator ;\n";
    if (!s.f.present("nfields"))
	*outf << "nfields 1\n";

    EST_Features::Entries p;
    for (p.begin(s.f); p; ++p)
	*outf << p->k << " " << p->v << endl;

    *outf << "#\n";
/*    if (f("timing_style") == "event")
        *outf << "timing_style event\n";
    else if (f("timing_style") == "unit")
        *outf << "timing_style unit\n";
*/
    
    for (ptr = s.head(); ptr != 0; ptr = ptr->next())
    {
	*outf << "\t";
	outf->precision(5);
	outf->setf(ios::fixed, ios::floatfield);
	outf->width(8);
	//	outf->fill('0');
	if (s.f("timing_style","0") == "event")
	    *outf << ptr->F("time",0);
	else
	    *outf << ptr->F("end",0);
	
	*outf << " 26 \t" << ptr->S("name","0");

	EST_Features f2;
	f2 = ptr->features();
	f2.remove("name");
	f2.remove("end");
	if (evaluate_ff)
	    evaluate(ptr,f2);

	if (f2.length() > 0)
	{
	    *outf << " ; ";
	    f2.save(*outf);
	}
	*outf << endl;
    }
    
    return write_ok;
}

EST_read_status load_ogi_label(EST_TokenStream &ts, EST_Relation &s)
{
    // This function reads OGI style label files. The start, end
    // time and names of the labels are mandatory. 
    EST_String key, val;
    float sr;
    int isr;
    
    // set up the character constant values for this stream
    ts.set_SingleCharSymbols(";");

    // Skip over header

    while(!ts.eof())
      {
	if ((ts.peek().col() == 0) && (ts.peek() == "END"))
	  {
	    if (ts.peek() == "END")
	      { // read rest of header
		ts.get();
		ts.get();
		ts.get();
	      }
	    break;
	  }
	key = ts.get().string();
	val = ts.get().string();
      }

    sr = 1000.0 / atof(val);
    isr = (int)sr;
    
    if (ts.eof())
    {
	cerr << "Error: couldn't find header in label file " 
	     << ts.filename() << endl;
	return wrong_format;
    }

    if (read_label_portion(ts, s, isr) == misc_read_error)
    {
	cerr << "error: in label file " << ts.filename() << " at line " <<
	    ts.linenum() << endl;
	return misc_read_error;
    }
    return format_ok;
}

EST_read_status load_words_label(EST_TokenStream &ts, EST_Relation &s)
{
    // This function reads label files in the form of simple word strings 
    // with no timing information.
    EST_Item *item;

    while (!ts.eof())
    {
	item = s.append();
	item->set("name",(EST_String)ts.get());
	item->set("end",0.0);
    }

    return format_ok;
}

static float convert_long_num_string_to_time(const char *s,int sample)
{
    // For those label files that think 100 nanosecond times are cool
    // we have to provide a special function to convert them as 
    // this quickly gets beyond the capabilities of ints.

    if (strlen(s) < 15)
	return atof(s)/sample;
    else
    {
	double a = 0,d;
	int i=0;
	for (i=0; 
	     (strchr(" \n\r\t",s[i]) != NULL) && (s[i] != '\0');
	     i++);

	for ( ;
	      (s[i] != '\0') && (s[i] >= '0') && (s[i] <= '9');
	      i++)
	{
	    a = a*10;
	    d = s[i]-'0';
	    a += (d/(double)sample);
	}
	return a;
    }
}

EST_read_status read_label_portion(EST_TokenStream &ts, EST_Relation &s, 
				   int sample)
{
    EST_Item *item;
    float hstart, hend;
    EST_String str;
    
    while(!ts.eof())
    {
	str = ts.get().string();
	if (str == ".")
	    return format_ok;
	
	item = s.append();
	
	hstart = convert_long_num_string_to_time(str,sample);
	str = ts.get().string();
	hend = convert_long_num_string_to_time(str,sample);
	
	item->set("end",hend);                   // time 
	item->set("name",ts.get().string());	 // name
	
	if (!ts.eoln())
	    item->set("rest_lab",ts.get_upto_eoln().string());
    }
    
    return format_ok;
}    

EST_read_status load_sample_label(EST_TokenStream &ts,
				  EST_Relation &s, int sample)
{
    
    if (sample == 0)	// maybe this should be an error
	sample = DEF_SAMPLE_RATE;
    
    // set up the character constant values for this stream
    ts.set_SingleCharSymbols(";");
    
    s.clear();
    if (read_label_portion(ts, s, sample) == misc_read_error)
    {
	cerr << "error: in label file " << ts.filename() << " at line " <<
	    ts.linenum() << endl;
	return misc_read_error;
    }
    return format_ok;
}

EST_write_status save_htk_label(const EST_String &filename, 
				const EST_Relation &a)
{
    ostream *outf;
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))
    {
	cerr << "save_htk_label: can't open label output file \"" << 
	    filename << "\"" << endl;
	return write_fail;
    }

    EST_write_status s = save_htk_label(outf, a);

    
    if (outf != &cout)
	delete outf;
    
    return s;
}

EST_write_status save_htk_label(ostream *outf,
				const EST_Relation &a)
{
    EST_Item *ptr;
    float end,start;
    
    outf->precision(6);

    start = end = 0;
    for (ptr = a.head(); ptr != 0; ptr = ptr->next())
    {
	outf->width(15);
	cout.setf(ios::left,ios::adjustfield);
	*outf << (int)(start * HTK_UNITS_PER_SECOND);
	outf->width(15);
	end = ptr->F("end",0.0);
	*outf << (int)(end * HTK_UNITS_PER_SECOND);
	*outf << " " << ptr->name() << endl;
	start = end;
    }

    return write_ok;
}

#if 0
EST_write_status save_label_spn(const EST_String &filename, 
				const EST_Relation &a)
{
    EST_Stream_Item *ptr;
    
    ostream *outf;
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))
    {
	cerr << "save_label_spn: can't open label output file \"" 
	    << filename << "\"" << endl;
	return write_fail;
    }
    
    ptr = a.head();
    outf->precision(3);
    outf->setf(ios::left, ios::adjustfield);
    outf->width(8);
    *outf << ptr->name();
    outf->setf(ios::fixed, ios::floatfield);
    outf->width(8);
    *outf << (ptr->dur() * 1000.0) << "\t (0,140)" << endl;
    
    for (; next(ptr) != 0; ptr = ptr->next())
    {
	outf->precision(3);
	outf->setf(ios::left, ios::adjustfield);
	outf->width(8);
	*outf << ptr->name();
	outf->setf(ios::fixed, ios::floatfield);
	outf->width(8);
	*outf << (ptr->dur() * 1000.0) << endl;
    }
    //    outf->precision(3);
    //    outf->setf(ios::left, ios::adjustfield);
    outf->width(8);
    *outf << ptr->name();
    outf->setf(ios::fixed, ios::floatfield);
    outf->width(8);
    *outf << (ptr->dur() * 1000.0) << "\t (99,80)" << endl;
    
    if (outf != &cout)
	delete outf;
    
    return write_ok;
}

EST_write_status save_label_names(const EST_String &filename, 
				  const EST_Relation &a, 
				  const EST_String &features)
{
    EST_Stream_Item *ptr;
    
    ostream *outf;
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))  
    {
	cerr << "save_label_name: can't open label output file \"" 
	    << filename << "\"" << endl;
	return misc_write_error;
    }
    
    for (ptr = a.head(); next(ptr) != 0; ptr = ptr->next())
    {
	*outf << ptr->name();
	if ((features != "") && (features != "OneLine"))
	    *outf << endl;
	else
	    *outf << " ";
    }
    
    *outf << ptr->name() << endl;
    
    if (outf != &cout)
	delete outf;
    return write_ok;
}
#endif

EST_write_status save_RelationList(const EST_String &filename, 
				   const EST_RelationList &plist, 
				   int time, int path)
{
    EST_Litem *p;
    EST_Item *ptr;
    EST_String outname;
    float start,end;
    
    ostream *outf;
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))
    {
	cerr << "save_StreamList: can't open MLF output file \"" 
	    << filename << "\"\n";
	return write_fail;
    }
    
    *outf << "#!MLF!#\n";	// MLF header/identifier
    outf->precision(6);

    start = end = 0;
    for (p = plist.head(); p != 0; p = p->next())
    {
	outname = path ? plist(p).name() : basename(plist(p).name());
	*outf << "\"*/" << outname<<"\"\n";
	for (ptr = plist(p).head(); ptr != 0; ptr = ptr->next())
	{
	    if (time)
	    {
		outf->width(15);
		cout.setf(ios::left,ios::adjustfield);
		*outf << (int)(start * HTK_UNITS_PER_SECOND);
		outf->width(15);
		end = ptr->F("end",0.0);
		*outf << (int)(end * HTK_UNITS_PER_SECOND) << " ";
		start = end;
	    }
	    *outf << ptr->S("name","0") << endl;
	}
	*outf << ".\n";
    }
    
    if (outf != &cout)
	delete outf;
    return write_ok;
}    

EST_write_status save_WordList(const EST_String &filename, 
			       const EST_RelationList &plist, 
			       int style)
{
    EST_Litem *p;
    EST_Item *ptr;
    
    ostream *outf;
    if (filename == "-")
	outf = &cout;
    else
	outf = new ofstream(filename);
    
    if (!(*outf))
    {
	cerr << "save:WordList: can't open WordList output file \"" 
	    << filename << "\"\n";
	return write_fail;
    }
    
    for (p = plist.head(); p != 0; p = p->next())
    {
	for (ptr = plist(p).head(); ptr->next() != 0; ptr = ptr->next())
	{
	    *outf << ptr->name();
	    if (style == 0)
		*outf << endl;
	    else
		*outf << " ";
	}
	if (ptr != 0)
	    *outf << ptr->name() << endl;
    }
    
    if (outf != &cout)
	delete outf;
    return write_ok;
}    

EST_write_status save_ind_RelationList(const EST_String &filename, 
				       const EST_RelationList &plist, 
				       const EST_String &features, 
				       int path)
{
    EST_Litem *p;
    EST_String outname;
    (void) filename;
    (void) features;
    
    for (p = plist.head(); p != 0; p = p->next())
    {
	outname = path ? plist(p).name() : basename(plist(p).name());
	if (plist(p).save(outname,false) != write_ok)
	    return misc_write_error;
    }
    
    return write_ok;
}    

EST_read_status load_RelationList(const EST_String &filename, 
				  EST_RelationList &plist)
{
    EST_TokenStream ts;
    EST_String fns, name;
    
    if (((filename == "-") ? ts.open(cin) : ts.open(filename)) != 0)
    {
	cerr << "Can't open label input file " << filename << endl;
	return misc_read_error;
    }
    // set up the character constant values for this stream
    ts.set_SingleCharSymbols(";");
    
    // Skip over header
    if (ts.get().string() != "#!MLF!#")
    {
	cerr << "Not MLF file\n";
	return wrong_format;
    }

    while(!ts.eof())
    {
	// put filename in as stream name. The filename is usually surrounded
	// by quotes, so remove these.
	fns = ts.get().string();
	strip_quotes(fns);
	EST_Relation s(fns);
	s.f.set("name", fns); // simonk
	plist.append(s);

    	if (read_label_portion(ts, plist.last(), 10000000) == misc_read_error)
	{
	    cerr << "error: in reading MLF file\n";
	    cerr << "section for file " << fns << 
		" at line " << ts.linenum() << " is badly formatted\n";
	    
	    return misc_read_error;
	}
    }

    return format_ok;
}

static void pad_ends(EST_Relation &s, float length)
{
    // add evenly spaced dummy end values to Relation
    EST_Item *p;
    int i;
    
    for (i = 0, p = s.head(); p; p = p->next(), ++i)
	p->set("end",(length * float(i)/float(s.length())));
}

EST_read_status read_RelationList(EST_RelationList &plist, 
				  EST_StrList &files, EST_Option &al)
{
    EST_Litem *p, *plp;
    
    if (al.val("-itype", 0) == "mlf")
    {
	if (load_RelationList(files.first(), plist) != format_ok)
	    exit (-1);
    }
    else
	for (p = files.head(); p; p = p->next())
	{
	    EST_Relation s(files(p));
	    plist.append(s);
	    plp = plist.tail();
	    if (al.present("-itype"))
	    {
		if (plist(plp).load(files(p), al.val("-itype")) != format_ok)
		    exit (-1);
	    }
	    else if (plist(plp).load(files(p)) != format_ok)
		exit (-1);
	    if ((al.val("-itype", 0) == "words") && (al.present("-length")))
		pad_ends(s, al.fval("-length"));
	    
	}
    
    return format_ok;
}
