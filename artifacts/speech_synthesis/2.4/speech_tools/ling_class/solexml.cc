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
 /* Code to reas SOLE format XML as utterances.                           */
 /*                                                                       */
 /*************************************************************************/

#include <cstdlib>
#include <cstdio>
#include "EST_THash.h"
#include "EST_error.h"
#include "solexml.h"
#include "rxp/XML_Parser.h"

static EST_Regex simpleIDRegex(".*#id(w\\([0-9]+\\))");
static EST_Regex rangeIDRegex(".*#id(w\\([0-9]+\\)).*id(w\\([0-9]+\\))");

class Parse_State
  {
public:
    int depth;
    EST_String relName;
    EST_Utterance *utt;
    EST_Relation *rel;
    EST_Item *parent;
    EST_Item *current;

    EST_THash<EST_String, EST_Item_Content *> contents;

    Parse_State() : contents(100) {}
  };

class Sole_Parser_Class : public XML_Parser_Class
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

EST_read_status solexml_read(FILE *file, 
			     const EST_String &name,
			     EST_Utterance &u,
			     int &max_id)
{
  (void)max_id;
  (void)print_attributes;	// just to shut -Wall up.
  Sole_Parser_Class pclass;
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

static void ensure_relation(Parse_State *state)
{
  if (state->rel==NULL)
    {
      state->rel = state->utt->create_relation(state->relName);
    }
}

static EST_Item_Content *get_contents(Parse_State *state, EST_String id)
{
  EST_Item_Content *c = state->contents.val(id);
  if (c==NULL)
    {
      c = new EST_Item_Content();
      state->contents.add_item(id, c);
    }

  return c;
}

static void extract_ids(XML_Attribute_List &attributes, 
			EST_TList<EST_String> &ids)
{
  EST_String val;
  static int count;
  if (attributes.present("id"))
    {
      val = attributes.val("id");
      ids.append(val);
    }
  else if (attributes.present("href"))
    {
      val = attributes.val("href");
      int starts[EST_Regex_max_subexpressions];
      int ends[EST_Regex_max_subexpressions];
      
      if (val.matches(simpleIDRegex, 0, starts, ends))
	{
	  EST_String n = val.at(starts[1], ends[1]-starts[1]);
	  
	  ids.append("w" + n);
	}
      else if (val.matches(rangeIDRegex, 0, starts, ends))
	{
	  int n1 = atoi(val.at(starts[1], ends[1]-starts[1]));
	  int n2 = atoi(val.at(starts[2], ends[2]-starts[2]));
	  
	  for(int i=n1; i<=n2; i++)
	    {
	      char buf[100];
	      sprintf(buf, "w%d", i);
	      
	      ids.append(buf);
	    }
	  
	}
      else
	EST_warning("element with bad ID or HREF '%s'", (const char *)val);
    }
  else
    {
      char buf[100];
      sprintf(buf, "n%d", ++count);
	  
      ids.append(buf);
      return;
    } 

}


/** Now we define the callbacks.
  */

void Sole_Parser_Class::document_open(XML_Parser_Class &c,
		      XML_Parser &p,
		      void *data)
{
  (void)c; (void)p; 
  Parse_State *state = (Parse_State *)data;

  state->depth=1;
  state->rel=NULL;
  state->parent=NULL;
  state->current=NULL;
}

void Sole_Parser_Class::document_close(XML_Parser_Class &c,
		    XML_Parser &p,
		    void *data)
{
  (void)c; (void)p; (void)data;
}


void Sole_Parser_Class::element_open(XML_Parser_Class &c,
		  XML_Parser &p,
		  void *data,
		  const char *name,
		  XML_Attribute_List &attributes)
{
  (void)c; (void)p; (void)attributes;
  Parse_State *state = (Parse_State *)data;

  state->depth++;

  if (strcmp(name, "solexml")==0)
    {
      state->relName=attributes.val("relation");
      printf("start solexml relation=%s\n", (const char *)state->relName);
      return;
    }
  else if (strcmp(name, "text-elem")==0)
    {
      // ignore these
      return;
    }

  ensure_relation(state);

  if (strcmp(name, "anaphora-elem")==0 
      || strcmp(name, "wordlist")==0
      || strcmp(name, "w")==0)
    {
      EST_TList<EST_String> ids;
      extract_ids(attributes, ids);

      EST_Litem *idp = ids.head();
      bool first=TRUE;
      for(; idp!= NULL; idp = idp->next())
	{
	  EST_String id = ids(idp);
	  if (id==EST_String::Empty)
	    XML_Parser_Class::error(c, p, data, EST_String("Element With No Id"));

	  if (first)
	    first=FALSE;
	  else
	    {
	      state->current = state->parent;
	      state->parent=state->parent->up();
	    }
	    

	  EST_Item_Content *cont = get_contents(state, id);

	  cont->set_name(id);
	  
	    XML_Attribute_List::Entries them;
	    for(them.begin(attributes); them ; them++)
	      {
		EST_String k = them->k;
		EST_String v = them->v;
		cont->f.set(k,v);
	      }

	  EST_Item *item;

	  if (state->current == NULL)
	    if (state->parent == NULL)
	      item = state->rel->append();
	    else
	      item = state->parent->insert_below();
	  else 
	    item = state->current->insert_after();

	  item->set_contents(cont);

	  state->current=NULL;
	  state->parent=item;
	}
    }
  else
    EST_warning("SOLE XML Parser: unknown element %s", name);
}


void Sole_Parser_Class::element(XML_Parser_Class &c,
				XML_Parser &p,
				void *data,
				const char *name,
				XML_Attribute_List &attributes)
{
  (void)c; (void)p; (void)attributes;
  Parse_State *state = (Parse_State *)data;

  if (strcmp(name, "language")==0)
    {
      state->utt->f.set("language", attributes.val("name"));
      return;
    }

  element_open(c, p, data, name, attributes);
  element_close(c, p, data, name);
}


void Sole_Parser_Class::element_close(XML_Parser_Class &c,
		   XML_Parser &p,
		   void *data,
		   const char *name)
{
  (void)c; (void)p; (void)name;
  Parse_State *state = (Parse_State *)data;

  if (strcmp(name, "anaphora-elem")==0 
      || strcmp(name, "wordlist")==0
      || strcmp(name, "w")==0)
    {
      state->depth--;
      state->current = state->parent;
      state->parent=state->parent->up();;
    }
}


void Sole_Parser_Class::pcdata(XML_Parser_Class &c,
	    XML_Parser &p,
	    void *data,
	    const char *chars)
{
  (void)c; 
  
 Parse_State *state = (Parse_State *)data;
 
 if (state->parent != NULL && p.context(0) == "w")
   state->parent->set(EST_String("word"), chars);
  
  //   printf("SOLE XML Parser [pcdata[%s]] %d\n", chars, state->depth);
}


void Sole_Parser_Class::cdata(XML_Parser_Class &c,
	   XML_Parser &p,
	   void *data,
	   const char *chars)
{
  (void)c; (void)p; (void)data; (void)chars;
  // Parse_State *state = (Parse_State *)data;

  //   printf("SOLE XML Parser [cdata[%s]] %d\n", chars, state->depth);
}


void Sole_Parser_Class::processing(XML_Parser_Class &c,
		XML_Parser &p,
		void *data,
		const char *instruction)
{
  (void)c; (void)p; 
  Parse_State *state = (Parse_State *)data;

  printf("SOLE XML Parser [proc[%s]] %d\n", instruction, state->depth);
}


void Sole_Parser_Class::error(XML_Parser_Class &c,
	   XML_Parser &p,
	   void *data)
{
  (void)c; (void)p;  (void)data;
  // Parse_State *state = (Parse_State *)data;

  EST_error("SOLE XML Parser %s", get_error(p));

  est_error_throw();
}
