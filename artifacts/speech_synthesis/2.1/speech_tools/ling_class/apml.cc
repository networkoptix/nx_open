 /************************************************************************/
 /*                                                                      */
 /*                Centre for Speech Technology Research                 */
 /*                     University of Edinburgh, UK                      */
 /*                       Copyright (c) 2002                             */
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
 /*                 Author: Rob Clark  (robert@cstr.ed.ac.uk)             */
 /* --------------------------------------------------------------------  */
 /* Code to read APML format XML as utterances.                           */
 /*                                                                       */
 /*************************************************************************/

#include <cstdlib>
#include <cstdio>
#include "EST_THash.h"
#include "EST_error.h"
#include "apml.h"
#include "rxp/XML_Parser.h"

static EST_Regex simpleIDRegex(".*#id(w\\([0-9]+\\))");
static EST_Regex rangeIDRegex(".*#id(w\\([0-9]+\\)).*id(w\\([0-9]+\\))");
static EST_Regex RXpunc("[\\.,\\?\\!\"]+");

class Parse_State
  {
public:
    int depth;
    int maxid;
    EST_Utterance *utt;
    EST_Relation *tokens;
    EST_Relation *perf;
    EST_Relation *com;
    EST_Relation *semstruct;
    EST_Relation *emphasis;
    EST_Relation *boundary;
    EST_Relation *pause;
    EST_Item *parent;
    EST_Item *pending;
    EST_Item *last_token;
  };

class Apml_Parser_Class : public XML_Parser_Class
{
protected:
  virtual void document_open(XML_Parser_Class &c,
			XML_Parser &p,
			void *data);
  virtual void document_close(XML_Parser_Class &c,
			 XML_Parser &p,
			 void *data);
  
  virtual void element_open(XML_Parser_Class &c,
		       XML_Parser &p,
		       void *data,
		       const char *name,
		       XML_Attribute_List &attributes);
  virtual void element(XML_Parser_Class &c,
		  XML_Parser &p,
		  void *data,
		  const char *name,
		  XML_Attribute_List &attributes);
  virtual void element_close(XML_Parser_Class &c,
			XML_Parser &p,
			void *data,
			const char *name);

  virtual void pcdata(XML_Parser_Class &c,
		 XML_Parser &p,
		 void *data,
		 const char *chars);
  virtual void cdata(XML_Parser_Class &c,
		XML_Parser &p,
		void *data,
		const char *chars);

  virtual void processing(XML_Parser_Class &c,
		     XML_Parser &p,
		     void *data,
		     const char *instruction);
  virtual void error(XML_Parser_Class &c,
		XML_Parser &p,
		void *data);
};

static void print_attributes(XML_Attribute_List &attributes)
{
  XML_Attribute_List::Entries them;

  for(them.begin(attributes); them ; them++)
    printf(" %s='%s'", 
	   (const char *)them->k, 
	   (const char *)them->v);
}

EST_read_status apml_read(FILE *file, 
			     const EST_String &name,
			     EST_Utterance &u,
			     int &max_id)
{
  (void)max_id;
  (void)print_attributes;	// just to shut -Wall up.
  Apml_Parser_Class pclass;
  Parse_State state;

  u.clear();

  state.utt=&u;

  XML_Parser *parser = pclass.make_parser(file, name, &state);
  parser->track_context(TRUE);

  CATCH_ERRORS()
    return read_format_error;

  parser->go();

  END_CATCH_ERRORS();

  return read_ok;
}



/** Now we define the callbacks.
  */

void Apml_Parser_Class::document_open(XML_Parser_Class &c,
		      XML_Parser &p,
		      void *data)
{
  (void)c; (void)p; 
  Parse_State *state = (Parse_State *)data;

  state->maxid=0;

  state->depth=1;
  state->parent=NULL;
  state->pending=NULL;
  state->last_token=NULL;

  // create relations:
  state->perf = state->utt->create_relation("Perfomative");
  state->com = state->utt->create_relation("Communicative");
  state->tokens = state->utt->create_relation("Token");
  state->semstruct = state->utt->create_relation("SemStructure");
  state->emphasis = state->utt->create_relation("Emphasis");
  state->boundary = state->utt->create_relation("Boundary");
  state->pause = state->utt->create_relation("Pause");


}

void Apml_Parser_Class::document_close(XML_Parser_Class &c,
		    XML_Parser &p,
		    void *data)
{
  (void)c; (void)p; (void)data;
}


void Apml_Parser_Class::element_open(XML_Parser_Class &c,
		  XML_Parser &p,
		  void *data,
		  const char *name,
		  XML_Attribute_List &attributes)
{
  (void)c; (void)p; (void)attributes;
  Parse_State *state = (Parse_State *)data;

  //cout << " In element_open: " << name << "\n";

  if (strcmp(name, "turnallocation")==0)
    {
      // currently ignore
      return;
    }

  if (strcmp(name, "apml")==0) 
    return;  // ignore

  state->depth++;

  if( strcmp(name, "performative")==0
      || strcmp(name, "rheme")==0
      || strcmp(name, "theme")==0
      || strcmp(name, "emphasis")==0
      || strcmp(name, "boundary")==0
      || strcmp(name, "pause")==0)
    {
      
      // create new item content
      EST_Item_Content *cont = new EST_Item_Content();
      cont->set_name(name);
	  
      XML_Attribute_List::Entries them;
      for(them.begin(attributes); them ; them++)
	{
	  EST_String k = them->k;
	  EST_String v = them->v;
	  cont->f.set(k,v);
	}

      EST_Item *item;
      
      if( strcmp(name, "emphasis")==0 )
	{
	  item = state->emphasis->append();
	  state->pending = item;
	}
      else if(strcmp(name, "boundary")==0 )
	{
	  item = state->boundary->append();
	  if(state->last_token)
	    item->append_daughter(state->last_token);
	}
      else if(strcmp(name, "pause")==0 )
	{
	  item = state->pause->append();
	  if(state->last_token)
	    item->append_daughter(state->last_token);
	}
      else
	{
	  if (state->parent == NULL)
	    item = state->semstruct->append();
	  else
	    item = state->parent->append_daughter();
	  state->parent=item;
	}

      item->set_contents(cont);
      
            
    }
  else
    EST_warning("APML Parser: unknown element %s", name);
}


void Apml_Parser_Class::element(XML_Parser_Class &c,
				XML_Parser &p,
				void *data,
				const char *name,
				XML_Attribute_List &attributes)
{
  (void)c; (void)p; (void)attributes;

  element_open(c, p, data, name, attributes);
  element_close(c, p, data, name);
}


void Apml_Parser_Class::element_close(XML_Parser_Class &c,
		   XML_Parser &p,
		   void *data,
		   const char *name)
{
  (void)c; (void)p; (void)name;
  Parse_State *state = (Parse_State *)data;

  if ( strcmp(name, "emphasis")==0
       || strcmp(name, "boundary")==0 
       || strcmp(name, "pause")==0 )
    {
      state->depth--;
      state->pending=NULL;
    }


  if (strcmp(name, "performative")==0 
      || strcmp(name, "theme")==0
      || strcmp(name, "rheme")==0)
    {
      state->depth--;
      state->pending = NULL;
      state->parent=state->parent->up();
    }
}


void Apml_Parser_Class::pcdata(XML_Parser_Class &c,
	    XML_Parser &p,
	    void *data,
	    const char *chars)
{
  (void)c; 
  
 Parse_State *state = (Parse_State *)data;
 EST_String strings[255];

 split(chars,strings,255,RXwhite);
 
 //   for(int cc=0 ; cc < 20 ; ++cc)
 //  cout << cc << ": \"" << strings[cc] << "\" (" << strings[cc].length() << ")\n";

 int s=0;

 while( s < 1 || strings[s].length() > 0 )
   {
     if(strings[s].length() > 0 )
       {
	 // Just Punctuation
	 if(strings[s].matches(RXpunc))
	   {
	     state->last_token->set("punc",strings[s]);
	   }
	 // Text and possibly punc
	 else	   
	   {
	     EST_Item_Content *cont = new EST_Item_Content();
	     EST_Item *item;
	     
	     if (state->parent == NULL)
	       item = state->semstruct->append();
	     else
	       item = state->parent->append_daughter();
	     item->set_contents(cont);
	     
	     // strip pre-punc here.
	     int i = strings[s].index(RXpunc);
	     EST_String ps = strings[s].at(RXpunc);
	     EST_String intermediate;
	     if( ps.length() > 0 && i == 0)
	       {
		 cout << "Got pre punc: " << ps << endl;
		 intermediate = strings[s].after(RXpunc);
		 // cont->set_name(strings[s].before(RXpunc));
		 item->set("prepunctuation",ps);
	       }
	     else
	       {
		 intermediate = strings[s];
		 item->set("prepunctuation","");
	       }
	     // now strip punc
	     ps = intermediate.at(RXpunc);
	     if( ps.length() > 0 )
	       {
		 cout << "Got punc: " << ps << endl;
		 cont->set_name(intermediate.before(RXpunc));
		 item->set("punc",ps);
	       }
	     else
	       {
		 cont->set_name(intermediate);
		 item->set("punc","");
	       }

	   state->tokens->append(item);
	   state->last_token = item;
	   
	   if(state->pending)
	     {
	       state->pending->append_daughter(item);
	     }
	   
	   //  if (state->parent != NULL && p.context(0) == "w")
	   //  state->parent->set(EST_String("token"), chars);
	   
	   //cout << "  got token: " << item->name() << "\n";
	   }
       }
     ++s;
   }
}


void Apml_Parser_Class::cdata(XML_Parser_Class &c,
	   XML_Parser &p,
	   void *data,
	   const char *chars)
{
  (void)c; (void)p; (void)data; (void)chars;
  // Parse_State *state = (Parse_State *)data;

  //   printf("APML XML Parser [cdata[%s]] %d\n", chars, state->depth);
}


void Apml_Parser_Class::processing(XML_Parser_Class &c,
		XML_Parser &p,
		void *data,
		const char *instruction)
{
  (void)c; (void)p; 
  Parse_State *state = (Parse_State *)data;

  printf("APML XML Parser [proc[%s]] %d\n", instruction, state->depth);
}


void Apml_Parser_Class::error(XML_Parser_Class &c,
	   XML_Parser &p,
	   void *data)
{
  (void)c; (void)p;  (void)data;
  // Parse_State *state = (Parse_State *)data;

  EST_error("APML Parser %s", get_error(p));

  est_error_throw();
}







