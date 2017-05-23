 /************************************************************************/
 /*                                                                      */
 /*                Centre for Speech Technology Research                 */
 /*                     University of Edinburgh, UK                      */
 /*                       Copyright (c) 1996,1997                        */
 /*                        All Rights Reserved.                          */
 /*                                                                      */
 /*  Permission is hereby granted, free of charge, to use and distribute */
 /*  this software and its documentation without restriction, including  */
 /*  without limitation the rights to use, copy, modify, merge, publish, */
 /*  distribute, sublicense, and/or sell copies of this work, and to     */
 /*  permit persons to whom this work is furnished to do so, subject to  */
 /*  the following conditions:                                           */
 /*   1. The code must retain the above copyright notice, this list of   */
 /*      conditions and the following disclaimer.                        */
 /*   2. Any modifications must be clearly marked as such.               */
 /*   3. Original authors' names are not deleted.                        */
 /*   4. The authors' names are not used to endorse or promote products  */
 /*      derived from this software without specific prior written       */
 /*      permission.                                                     */
 /*                                                                      */
 /*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK       */
 /*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING     */
 /*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT  */
 /*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE    */
 /*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES   */
 /*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN  */
 /*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,         */
 /*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF      */
 /*  THIS SOFTWARE.                                                      */
 /*                                                                      */
 /*************************************************************************/
 /*                                                                       */
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)             */
 /* --------------------------------------------------------------------  */
 /* Functions to load and save utterances in various formats.             */
 /*                                                                       */
 /*************************************************************************/

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include "EST_string_aux.h"
#include "EST_FileType.h"
#include "EST_Token.h"
#include "ling_class/EST_Utterance.h"
#include "EST_UtteranceFile.h"

static EST_read_status load_all_contents(EST_TokenStream &ts,
//					 EST_THash<int,EST_Val> &sitems,
					 EST_TVector < EST_Item_Content * > &sitems,
					 int &max_id);
static EST_read_status load_relations(EST_TokenStream &ts,
				      EST_Utterance &utt,
				      const EST_TVector < EST_Item_Content * > &sitems
//				      const EST_THash<int,EST_Val> &sitems
				      );
// static EST_write_status save_est_ascii(ostream &outf,const EST_Utterance &utt);
static EST_write_status utt_save_all_contents(ostream &outf,
					      const EST_Utterance &utt,
					      EST_TKVL<void *,int> &sinames);
static EST_write_status utt_save_all_contents(ostream &outf,
					      EST_Item *n, 
					      EST_TKVL<void *,int> &sinames,
					      int &si_count);
static EST_write_status utt_save_ling_content(ostream &outf,
					      EST_Item *si,
					      EST_TKVL<void *,int> &sinames,
					      int &si_count);

static void node_tidy_up(int &k, EST_Item_Content *node)
{
    // Called to delete the nodes in the hash table when a load
    (void)k;

    if (node->unref_relation("__READ__"))
      delete node;
}

EST_read_status EST_UtteranceFile::load_est_ascii(EST_TokenStream &ts,
						  EST_Utterance &u, 
						  int &max_id)
{
    EST_Option hinfo;
    bool ascii;
    EST_EstFileType t;
    EST_read_status r;
    //    EST_THash<int,EST_Val> sitems(100);

    EST_TVector< EST_Item_Content * > sitems(100);

    // set up the character constant values for this stream
    ts.set_SingleCharSymbols(";()");
    ts.set_quotes('"','\\');

    if ((r = read_est_header(ts, hinfo, ascii, t)) != format_ok)
	return r;
    if (t != est_file_utterance)
	return misc_read_error;
    if (hinfo.ival("version") != 2)
    {
      if (hinfo.ival("version") == 3)
	EST_warning("Loading est utterance format version 3, ladders will not be understood");
      else
	{
	  EST_error("utt_load: %s  wrong version of utterance format expected 2 (or 3) but found %d",  
		    (const char *)ts.pos_description(), hinfo.ival("version"));
	}
    }

    // Utterance features
    if (ts.get() != "Features")
    {
	cerr << "utt_load: " << ts.pos_description() <<
	    " missing utterance features section" << endl;
	return misc_read_error;
    }
    else
	u.f.load(ts);
    // items
    if (ts.get() != "Stream_Items")
    {
	cerr << "utt_load: " << ts.pos_description() <<
	    " missing Items section" << endl;
	return misc_read_error;
    }
    max_id = 0;
    r = load_all_contents(ts, sitems, max_id);

    // Only exist in older form utterances so soon wont be necessary
    if (ts.peek() == "Streams")
    {
	cerr << "utt.load: streams found in utterance file, " <<
	    "no longer supported" << endl;
	return misc_read_error;
    }

    // Relations
    if ((r == format_ok) && (ts.get() != "Relations"))
    {
	cerr << "utt_load: " << ts.pos_description() <<
	    " missing Relations section" << endl;
	return misc_read_error;
    }

    r = load_relations(ts, u, sitems);

    if ((r == format_ok) && (ts.get() != "End_of_Utterance"))
    {
	cerr << "utt_load: " << ts.pos_description() <<
	    " End_of_Utterance expected but not found" << endl;
	return misc_read_error;
    }

    //    if (r != format_ok)
    //    {
	// This works because even if some of these si's have been
	// linked to nodes they will be unlink when the si is destroyed
    for(int ni=0; ni < sitems.length(); ni++)
      {
	EST_Item_Content *c = sitems[ni];
	if (c != NULL)
	  node_tidy_up(ni, c);
      }
	//    }

    return r;

}

static EST_read_status load_all_contents(EST_TokenStream &ts,
//					 EST_THash<int,EST_Val> &sitems,
					 EST_TVector < EST_Item_Content * > &sitems,
					 int &max_id)
{
    // Load items into table with names for later reference
    // by relations
    EST_String Sid;
    bool ok;
    int id,idval;

    while (ts.peek() != "End_of_Stream_Items")
    {
	EST_Item_Content *si = new EST_Item_Content;

	si->relations.add_item("__READ__", est_val((EST_Item *)NULL), 1);

	id = 0;

	Sid = ts.get().string();

	id = Sid.Int(ok);
	if (!ok)
	{
	    cerr << "utt_load: " << ts.pos_description() << 
		" Item name not a number: " << Sid << endl;
	    return misc_read_error;
	}
	if (id >= sitems.length())
	  {
	    sitems.resize(id*2, 1);
	  }
	sitems[id] = si;
	//	sitems.add_item(id,est_val(si));
	if (si->f.load(ts) != format_ok)
	    return misc_read_error;
	idval = si->f.I("id",0);
	if (idval > max_id)
	    max_id = idval;
	if (ts.eof())
	    return misc_read_error;  // just in case this happens
    }

    ts.get(); // skip "End_of_Stream_Items"

    return format_ok;
}

static EST_read_status load_relations(EST_TokenStream &ts,
				      EST_Utterance &utt,
				      const EST_TVector < EST_Item_Content * > &sitems
//				      const EST_THash<int,EST_Val> &sitems
				      )
{
    // Load relations

    while (ts.peek() != "End_of_Relations")
    {
	// can't use create relation as we don't know its name until
	// after its loaded
	EST_Relation *r = new EST_Relation;

	if (r->load(ts,sitems) != format_ok)
	    return misc_read_error;

	r->set_utt(&utt);
	utt.relations.set_val(r->name(),est_val(r));

	if (ts.eof())
	    return misc_read_error;
    }

    ts.get();  // Skip "End_of_Relations"

    return format_ok;
}
	

EST_write_status EST_UtteranceFile::save_est_ascii(ostream &outf,const EST_Utterance &utt)
{
    EST_write_status v = write_ok;
    
    outf.precision(8);
    outf.setf(ios::fixed, ios::floatfield);
    outf.width(8);
    
    outf << "EST_File utterance\n"; // EST header identifier.
    outf << "DataType ascii\n";
    outf << "version 2\n";
    outf << "EST_Header_End\n"; // EST end of header identifier.

    // Utterance features
    outf << "Features ";
    utt.f.save(outf);
    outf << endl;

    outf << "Stream_Items\n";
    EST_TKVL<void *,int> sinames;
    v = utt_save_all_contents(outf,utt,sinames);
    if (v == write_fail) return v;
    outf << "End_of_Stream_Items\n";

    // Relations
    outf << "Relations\n";
    EST_Features::Entries p;
    for (p.begin(utt.relations); p; p++)
    {
	v = relation(p->v)->save(outf,sinames);
	if (v == write_fail) return v;
    }
    outf << "End_of_Relations\n";

    outf << "End_of_Utterance\n";
    return write_ok;
}

static EST_write_status utt_save_all_contents(ostream &outf,
					      const EST_Utterance &utt,
					      EST_TKVL<void *,int> &sinames)
{
    // Write out all stream items in the utterance, as they may appear in
    // various places in an utterance keep a record of which ones
    // have been printed and related them to names for reference by
    // the Relations (and older Stream architecture).
    int si_count = 1;
    EST_write_status v = write_ok;

    // Find the stream items in the relations
    EST_Features::Entries p;
    for (p.begin(utt.relations); p; p++)
    {
	v = utt_save_all_contents(outf,relation(p->v)->head(),
				  sinames,si_count);
	if (v == write_fail) return v;
    }

    return v;
}

static EST_write_status utt_save_all_contents(ostream &outf,
					      EST_Item *n, 
					      EST_TKVL<void *,int> &sinames,
					      int &si_count)
{
    if (n == 0)
	return write_ok;
    else 
    {
	utt_save_ling_content(outf,n,sinames,si_count);
	// As we have more complex structures this will need to
	// be updated (i.e. we'll need a marking method for nodes)
	utt_save_all_contents(outf,n->next(),sinames,si_count);
	utt_save_all_contents(outf,n->down(),sinames,si_count);
    }
    return write_ok;
}

static EST_write_status utt_save_ling_content(ostream &outf,
					      EST_Item *si,
					      EST_TKVL<void *,int> &sinames,
					      int &si_count)
{
    // Save item and features if not already saved
					     
    if ((si != 0) && (!sinames.present(si->contents())))
    {
	sinames.add_item(si->contents(),si_count);
	outf << si_count << " ";
	si->features().save(outf);
	outf << endl;
	si_count++;
    }
    return write_ok;
}

EST_read_status EST_UtteranceFile::load_xlabel(EST_TokenStream &ts,
					       EST_Utterance &u, 
					       int &max_id)
{
  (void)max_id;
  EST_read_status status = read_ok;

  u.clear();

  EST_Relation *rel = u.create_relation("labels");

  status = rel->load("", ts, "esps");

  EST_Item *i = rel->head();
  float t=0.0;

  while (i != NULL)
    {
      i->set("start", t);
      t = i->F("end");
      i = i->next();
    }

  return status;
}

EST_write_status EST_UtteranceFile::save_xlabel(ostream &outf,
						const EST_Utterance &utt)
{
  EST_write_status status = write_error;

  EST_Relation *rel;

  EST_Features::Entries p;

  for (p.begin(utt.relations); p; p++)
    {
      rel = ::relation(p->v);

      EST_Item * hd = rel->head();

      
      while (hd)
	{
	  if (hd->up() || hd->down())
	    break;
	  hd=hd->next();
	}

      // didn't find anything => this is linear
      if(!hd)
	  return rel->save(outf, "esps", 0);
    }

  // Found no linear relations
  
  return status;
}

#if defined(INCLUDE_XML_FORMATS)

#include "genxml.h"
#include "apml.h"

// APML support
EST_read_status EST_UtteranceFile::load_apml(EST_TokenStream &ts,
						EST_Utterance &u, 
						int &max_id)
{
  FILE *stream;

  if ((stream=ts.filedescriptor())==NULL)
    return read_error;

  long pos=ftell(stream);

  {
  char buf[80];

  fgets(buf, 80, stream);

  if (strncmp(buf, "<?xml", 5) != 0)
    return read_format_error;

  fgets(buf, 80, stream);

  if (strncmp(buf, "<!DOCTYPE apml", 14) != 0)
    return read_format_error;
  }

  fseek(stream, pos, 0);

  EST_read_status stat = apml_read(stream, ts.filename(),u, max_id);

  if (stat != read_ok)
    fseek(stream, pos, 0);

  return stat;
}


// GenXML support

EST_read_status EST_UtteranceFile::load_genxml(EST_TokenStream &ts,
						EST_Utterance &u, 
						int &max_id)
{
  FILE *stream;

  if ((stream=ts.filedescriptor())==NULL)
    return read_error;

  long pos=ftell(stream);

  {
  char buf[80];

  fgets(buf, 80, stream);

  if (strncmp(buf, "<?xml", 5) != 0)
    return read_format_error;
  }

  fseek(stream, pos, 0);

  EST_read_status stat = EST_GenXML::read_xml(stream, ts.filename(),u, max_id);

  if (stat != read_ok)
    fseek(stream, pos, 0);

  return stat;
}

EST_write_status EST_UtteranceFile::save_genxml(ostream &outf,
						const EST_Utterance &utt)
{
  EST_write_status status=write_ok;

  EST_TStringHash<int> features(20);

  EST_Features::Entries p;

  for (p.begin(utt.relations); p; ++p)
    {
      EST_Relation *rel = ::relation(p->v);

      EST_Item * hd = rel->head();
      
      while (hd)
	{
	  EST_Features::Entries fp;
	  for (fp.begin(hd->features()); fp; ++fp)
	    features.add_item(fp->k, 1);
	  hd=hd->next();
	}
    }

  outf << "<?xml version='1.0'?>\n";

  outf << "<!DOCTYPE utterance PUBLIC '//CSTR EST//DTD cstrutt//EN' 'cstrutt.dtd'\n\t[\n";

  EST_TStringHash<int>::Entries f;

  outf << "\t<!ATTLIST item\n";
  for (f.begin(features); f; ++f)
    {
      if (f->k != "id")
	{
	  outf << "\t\t" << f->k << "\tCDATA #IMPLIED\n";
	}
    }

  outf << "\t\t>\n";

  outf << "\t]>\n";

  outf << "<utterance>\n";

  outf << "<language name='unknown'/>\n";

  for (p.begin(utt.relations); p; ++p)
    {
      EST_Relation *rel = ::relation(p->v);

      EST_Item * hd = rel->head();

      
      while (hd)
	{
	  if (hd->up() || hd->down())
	    break;
	  hd=hd->next();
	}

      // didn't find anything => this is linear
      if(!hd)
	{
	  outf << "<relation name='"<< rel->name()<< "' structure-type='list'>\n";

	  hd = rel->head();
	  while (hd)
	    {
	      outf << "    <item\n";

	      EST_Features::Entries p;
	      for (p.begin(hd->features()); p; ++p)
		if (p->k != "estContentFeature")
		  outf << "         " << p->k << "='" << p->v << "'\n";

	      outf << "         />\n";
	      
	      hd=hd->next();
	    }
	  
	  outf << "</relation>\n";
	}
      else // for now give an error for non-linear relations
	status=write_partial;
    }
  

  outf << "</utterance>\n";

  return status;
;
}
#endif

EST_String EST_UtteranceFile::options_short(void)
{
    EST_String s("");
    
    for(int n=0; n< EST_UtteranceFile::map.n() ; n++)
    {
      EST_UtteranceFileType type = EST_UtteranceFile::map.nth_token(n);
      if (type != uff_none)
	{
	  for(int ni=0; ni<NAMED_ENUM_MAX_SYNONYMS; ni++)
	    {
	      const char *nm = EST_UtteranceFile::map.name(type, ni);
	      if (nm==NULL)
		break;
	
	      if (s != "")
		s += ", ";
	      
	      s += nm;
	    }
	}
    }
    return s;
}

EST_String EST_UtteranceFile::options_supported(void)
{
    EST_String s("Available utterance file formats:\n");
    
    for(int n=0; n< EST_UtteranceFile::map.n() ; n++)
    {
      EST_UtteranceFileType type = EST_UtteranceFile::map.nth_token(n);
      if (type != uff_none)
	{
	  const char *d = EST_UtteranceFile::map.info(type).description;
	  for(int ni=0; ni<NAMED_ENUM_MAX_SYNONYMS; ni++)
	    {
	      const char *nm = EST_UtteranceFile::map.name(type, ni);
	      if (nm==NULL)
		break;
	
	      s += EST_String::cat("        ", (nm?nm:"NULL"), EST_String(" ")*(12-strlen((nm?nm:"NULL"))), (d?d:"NULL"), "\n");
	    }
	}
    }
    return s;
}



// note the order here defines the order in which loads are tried.
Start_TNamedEnumI_T(EST_UtteranceFileType, EST_UtteranceFile::Info, EST_UtteranceFile::map, utterancefile)
  { uff_none,		{ NULL }, 
    { FALSE, NULL, NULL, "unknown utterance file type"} },
  { uff_est,		{ "est", "est_ascii"}, 
    { TRUE, EST_UtteranceFile::load_est_ascii,  EST_UtteranceFile::save_est_ascii, "Standard EST Utterance File" } },
#if defined(INCLUDE_XML_FORMATS)
  { uff_apml,		{ "apml", "xml"}, 
    { TRUE, EST_UtteranceFile::load_apml,  NULL, "Utterance in APML" } },
  { uff_genxml,		{ "genxml", "xml"}, 
    { TRUE, EST_UtteranceFile::load_genxml,  EST_UtteranceFile::save_genxml, "Utterance in XML, Any DTD" } },
#endif
  { uff_xlabel,	{ "xlabel"}, 
    { TRUE, EST_UtteranceFile::load_xlabel,  EST_UtteranceFile::save_xlabel, "Xwaves Label File" } },
  { uff_none,		{NULL},
      { FALSE, NULL, NULL, "unknown utterance file type"} }

End_TNamedEnumI_T(EST_UtteranceFileType, EST_UtteranceFile::Info, EST_UtteranceFile::map, utterancefile)

Declare_TNamedEnumI(EST_UtteranceFileType, EST_UtteranceFile::Info)

#if defined(INSTANTIATE_TEMPLATES)
#include "../base_class/EST_TNamedEnum.cc"
Instantiate_TNamedEnumI(EST_UtteranceFileType, EST_UtteranceFile::Info)
#endif

Declare_TVector_Base_T(EST_Item_Content *, NULL, NULL, EST_Item_ContentP)

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TSimpleVector.cc"
#include "../base_class/EST_TVector.cc"
#include "../base_class/EST_Tvectlist.cc"

Instantiate_TVector_T(EST_Item_Content *, EST_Item_ContentP)

#endif

