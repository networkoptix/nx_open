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
 /* Code to read utterances marked up in XML according to a DTD with      */
 /* certain conventions indicating the mapping from XML to Utterance.     */
 /*                                                                       */
 /*************************************************************************/

#include <cstdlib>
#include <cstdio>
#include <cctype>
#include "EST_TDeque.h"
#include "EST_THash.h"
#include "EST_error.h"
#include "genxml.h"
#include "rxp/XML_Parser.h"

#include "ling_class_init.h"

#if defined(ESTLIBDIRC)
#    define __STRINGIZE(X) #X
#    define ESTLIBDIR __STRINGIZE(ESTLIBDIRC)
#endif


static EST_Regex simpleIDRegex("[^#]*#id(\\([-a-z0-9]+\\))");
static EST_Regex rangeIDRegex("[^#]*#id(\\([a-z]*\\)\\([0-9]*\\)\\(-\\([0-9]+\\)\\)*).*id(\\([a-z]*\\)\\([0-9]*\\)\\(-\\([0-9]+\\)\\)*)");
static EST_Regex featureDefRegex("\\([^:]*\\):\\(.*\\)");

// Separator between feature names in attributes.

static EST_String feat_sep(",");

// I'd like to get rid of this. It is a maximum for the number of features
// which can be named in an attribute, say for copying to the utterance.

#define MAX_FEATS (50)

// Parse state.

class GenXML_Parse_State
  {
public:
    int depth;
    int open_depth;
    int rel_start_depth;
    EST_TDeque<int> depth_stack;
    EST_String relName;
    bool linear;
    EST_Utterance *utt;
    EST_Relation *rel;
    EST_Item *parent;
    EST_Item *current;
    EST_String contentAttr;

    // used to force a given ID on a node.
    EST_String id;

    EST_TStringHash<EST_Item_Content *> contents;

    
    GenXML_Parse_State()  : contents(100) {}
  };

class GenXML_Parser_Class : public XML_Parser_Class
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

static void print_attributes(XML_Attribute_List &attributes);

XML_Parser_Class *EST_GenXML::pclass;


void EST_GenXML::class_init(void)
{
  ling_class_init::use();

  pclass = new GenXML_Parser_Class();
#ifdef DEBUGGING
  printf("Register estlib in genxml %s\n",  ESTLIBDIR "/\\1.dtd");
#endif
  
  pclass->register_id("//CSTR EST//DTD \\(.*\\)//[A-Z]*",
		      ESTLIBDIR "/\\1.dtd");
  pclass->register_id("//CSTR EST//ENTITIES \\(.*\\)//[A-Z]*",
		      ESTLIBDIR "/\\1.ent");
}

void EST_GenXML::register_id(const EST_String pattern, 
			const EST_String result)
{
  EST_GenXML::pclass->register_id(pattern, result);
}

void EST_GenXML::registered_ids(EST_StrList &list)
{
  EST_GenXML::pclass->registered_ids(list);
}

InputSource EST_GenXML::try_and_open(Entity ent)
{
  return EST_GenXML::pclass->try_and_open(ent);
}


EST_read_status EST_GenXML::read_xml(FILE *file, 
				     const EST_String &name,
				     EST_Utterance &u,
				     int &max_id)
{
  (void)max_id;
  (void)print_attributes;	// just to shut -Wall up.
  GenXML_Parse_State state;

  u.clear();

  state.utt=&u;

  XML_Parser *parser = EST_GenXML::pclass->make_parser(file, name, &state);
  parser->track_context(TRUE);

  CATCH_ERRORS()
    return read_format_error;

  parser->go();

  END_CATCH_ERRORS();

  return read_ok;
}

static void ensure_relation(GenXML_Parse_State *state, EST_String name)
{
  if (state->rel!=NULL && name == state->relName)
	return;

  state->rel = state->utt->create_relation(state->relName=name);
}

static EST_Item_Content *get_contents(GenXML_Parse_State *state, EST_String id)
{
  EST_Item_Content *c = state->contents.val(id);

  if (c==NULL)
    {
      c = new EST_Item_Content();
      state->contents.add_item(id, c);
      c->f.set("id", id);
    }
  else
    {
      if (c->relations.present(state->relName))
	return NULL;
    }

  return c;
}

static EST_String make_new_id(const char *root)
{
  char buf[100];
  static int count=0;

  sprintf(buf, "%s%d", root, ++count);
  return buf;
}


static void extract_ids(XML_Attribute_List &attributes, 
			EST_TList<EST_String> &ids)
{
  EST_String val;
  if (attributes.present("id"))
    {
      val = attributes.val("id");
#if defined(EST_DEBUGGING)
	  fprintf(stderr, "ID %s\n", (const char *)val);
#endif
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
#if defined(EST_DEBUGGING)
	  fprintf(stderr, "SIMPLE %s\n", (const char *)n);
#endif
	  ids.append(n);
	}
      else if (val.matches(rangeIDRegex, 0, starts, ends))
	{
	  EST_String prefix1 = val.at(starts[1], ends[1]-starts[1]);
	  int n1 = atoi(val.at(starts[2], ends[2]-starts[2]));
	  EST_String postfix1 = val.at(starts[4], ends[4]-starts[4]);
	  EST_String prefix2 = val.at(starts[5], ends[5]-starts[5]);
	  int n2 = atoi(val.at(starts[6], ends[6]-starts[6]));
	  EST_String postfix2 = val.at(starts[8], ends[8]-starts[8]);

#if defined(EST_DEBUGGING)
	  fprintf(stderr, "RANGE '%s' %d - '%s' // '%s' %d - '%s'\n",
		 (const char *)prefix1,
		 n1,
		 (const char *)postfix1,
		 (const char *)prefix2,
		 n2,
		 (const char *)postfix2
		 );
#endif

	  if (prefix1==prefix2)
	    prefix2="";
	  
	  char buf[100];
	  if (n1==n2)
	    {
	      int c;
	      if (postfix1.length()==0)
		{
		  sprintf(buf, "%s%s%d", 
			  (const char *)prefix1, 
			  (const char *)prefix2, 
			  n1
			  );
		  ids.append(buf);
		  c=1;
		}
	      else
		c=atoi(postfix1);
	      
	      if (postfix2.length()>0)
		for (; c<=atoi(postfix2); c++)
		  {
		    sprintf(buf, "%s%s%d-%d", 
			    (const char *)prefix1, 
			    (const char *)prefix2, 
			    n1,
			    c
			    );
		    ids.append(buf);
		  }
	    }
	  else
	    {
	      for(int i=n1; i<=n2; i++)
		{
		  if (i==n2
		      && postfix2.length()>0)
		    {
		      sprintf(buf, "%s%s%d", 
			      (const char *)prefix1, 
			      (const char *)prefix2, 
			      i
			      );
		      ids.append(buf);
		      for (int c=1; c<=atoi(postfix2); c++)
			{
			  sprintf(buf, "%s%s%d-%d", 
				  (const char *)prefix1, 
				  (const char *)prefix2, 
				  i,
				  c
				  );
			  ids.append(buf);
			}
		    }
		  else
		    {
		      if ( postfix1.length()>0)
			sprintf(buf, "%s%s%d-%s", 
				(const char *)prefix1, 
				(const char *)prefix2, 
				i,
				(const char *)postfix1
				);
		      else
			sprintf(buf, "%s%s%d", 
				(const char *)prefix1, 
				(const char *)prefix2, 
				i
				);
		      
		      ids.append(buf);
		    }
		  postfix1="";
		}
	      
	    }
	}
      else
	EST_warning("element with bad ID or HREF '%s'", (const char *)val);
    }
  else
    ids.append(make_new_id("n"));
  
  // cout << ids << "\n";
}

/* For debugging.
 */
static void print_attributes(XML_Attribute_List &attributes)
{
  XML_Attribute_List::Entries them;

  for(them.begin(attributes); them ; them++)
    printf(" %s='%s'", 
	   (const char *)them->k, 
	   (const char *)them->v);
}

/** Now we define the callbacks.
  */

void GenXML_Parser_Class::document_open(XML_Parser_Class &c,
		      XML_Parser &p,
		      void *data)
{
  (void)c; (void)p; 
  GenXML_Parse_State *state = (GenXML_Parse_State *)data;

  state->depth=1;
  state->open_depth=-1;
  state->rel_start_depth=-1;
  state->depth_stack.clear();
  state->rel=NULL;
  state->parent=NULL;
  state->current=NULL;
  state->id="";
}

void GenXML_Parser_Class::document_close(XML_Parser_Class &c,
		    XML_Parser &p,
		    void *data)
{
  (void)c; (void)p; (void)data;
}

static void proccess_features(EST_String name,
			     EST_String defs,
			     XML_Attribute_List &attributes,
			     EST_Features &f)
{
  EST_String names[MAX_FEATS];
  int starts[EST_Regex_max_subexpressions];
  int ends[EST_Regex_max_subexpressions];
  
  int n = split(defs, names, MAX_FEATS, feat_sep);
  for(int i=0; i<n; i++)
    {
      EST_String def = names[i];
      EST_String feat;
      EST_String attr;
      
      if (def.matches(featureDefRegex, 0, starts, ends))
	{
	  feat = def.at(starts[1], ends[1]-starts[1]);
	  attr = def.at(starts[2], ends[2]-starts[2]);
	}
      else
	{
	  attr=def;
	  feat=EST_String::cat(name, "_", attr);
	}
      
      EST_String fval = attributes.val(attr);
      
#ifdef DEBUGGING
      printf("on %s got %s(%s)=%s\n", name, 
	     (const char *)feat,
	     (const char *)attr, 
	     (const char *)fval);
#endif
      if (fval != EST_String::Empty)
	f.set(feat, fval);
    }
}

void GenXML_Parser_Class::element_open(XML_Parser_Class &c,
		  XML_Parser &p,
		  void *data,
		  const char *name,
		  XML_Attribute_List &attributes)
{
  (void)c; (void)p; (void)attributes; (void)name;
  GenXML_Parse_State *state = (GenXML_Parse_State *)data;

  state->depth++;

  EST_String val, ig;

  // Features to copy to utterance
  if (state->utt != NULL 
      && (val=attributes.val("estUttFeats")) != EST_String::Empty)
    proccess_features(name, val, attributes, state->utt->f);

  // Features to copy to relation
  if (state->rel != NULL 
      && (val=attributes.val("estRelFeats")) != EST_String::Empty)
    proccess_features(name, val, attributes, state->rel->f);


  if ((val=attributes.val("estRelationElementAttr")) != EST_String::Empty)
    {
      // All nodes inside this element are in the given relation
      EST_String relName = attributes.val(val);

      if (relName == EST_String::Empty)
	{
	  relName = "UNNAMED";
	  EST_warning("%s\nNo feature '%s' to name relation\n", get_error(p), (const char *)val);
	}

      EST_String relationType = attributes.val("estRelationTypeAttr");

      ensure_relation(state, relName);
      state->rel_start_depth=state->depth;
      state->linear=(attributes.val(relationType) == "linear"||
		     attributes.val(relationType) == "list");
#ifdef DEBUGGING
      printf("start of relation depth=%d name=%s type=%s\n", state->depth, (const char *)relName, state->linear?"linear":"tree");
#endif
    }
  else if ((state->rel_start_depth >= 0 && 
	    (ig=attributes.val("estRelationIgnore")) == EST_String::Empty)
	   || (val=attributes.val("estRelationNode")) != EST_String::Empty)
    {
      // This node defines an Item in a relation.
#ifdef DEBUGGING
      printf("push depth=%d name=%s ig=%s\n", state->depth, name, (const char *)ig);
#endif
      if (val != EST_String::Empty)
	ensure_relation(state, val);

      state->depth_stack.push(state->open_depth);
      state->open_depth=state->depth;

      EST_TList<EST_String> ids;

      if (state->id == EST_String::Empty)
	{
	  extract_ids(attributes, ids);
	}
      else
	ids.append(state->id);

      switch (ids.length())
	{
	case 0:
	  XML_Parser_Class::error(c, p, data, EST_String("Element With No Id"));
	  break;
	case 1:
	  {
	    EST_String id = ids.first();

	    if (id==EST_String::Empty)
	      XML_Parser_Class::error(c, p, data, EST_String("Element With No Id"));

	    EST_Item_Content *cont = get_contents(state, id);

	    if (!cont)
	      XML_Parser_Class::error(c, p, data, EST_String("Repeated Id ") + id);

	    XML_Attribute_List::Entries them;
	    for(them.begin(attributes); them ; them++)
	      {
		EST_String k = them->k;
		EST_String v = them->v;
		cont->f.set(k,v);
	      }

	    cont->f.set("id", id);

	    EST_Item *item;
	  
	    if (state->linear)
	      if (state->current == NULL)
		item = state->rel->append();
	      else
		item = state->current->insert_after();
	    else if (state->current == NULL)
	      if (state->parent == NULL)
		item = state->rel->append();
	      else
		item = state->parent->append_daughter();
	    else 
	      if (state->parent == NULL)
		item = state->current->insert_after();
	      else
		item = state->parent->append_daughter();
	  
	    item->set_contents(cont);
	    
	    state->current=NULL;
	    state->parent=item;
	  }
	  break;

	default:
	  {
	    bool embed = (attributes.val("estExpansion") == "embed");
	    if (embed)
	      {
		state->id=make_new_id("e");
		element_open(c, p, data, name, attributes);
		state->id="";
	      }
	    EST_Litem *idp = ids.head();
	    bool first=TRUE;
	    for(; idp!= NULL; idp = idp->next())
	      {
		 EST_String id = ids(idp);
		 if (id==EST_String::Empty)
		   XML_Parser_Class::error(c, p, data, EST_String("Element With No Id"));

		 if (!first)
		   element_close(c, p, data, name);
		 else
		   first=FALSE;

		 state->id=id;
		 element_open(c, p, data, name, attributes);
		 state->id=EST_String::Empty;
	      }
	    if (embed)
	      {
		element_close(c, p, data, name);
	      }
	  }
	}


      if (state->parent!=NULL)
	state->contentAttr = attributes.val("estContentFeature");
      
#ifdef DEBUGGING
      printf("\t current=%s parent=%s contA=%s\n", 
	     (const char *)state->current->name(),
	     (const char *)state->parent->name(),
	     (const char *)state->contentAttr);
#endif

    }
  else
    ; // Skip

}


void GenXML_Parser_Class::element(XML_Parser_Class &c,
				XML_Parser &p,
				void *data,
				const char *name,
				XML_Attribute_List &attributes)
{
  (void)c; (void)p; (void)attributes;
  GenXML_Parse_State *state = (GenXML_Parse_State *)data;
  (void)state;

  element_open(c, p, data, name, attributes);
  element_close(c, p, data, name);
}


void GenXML_Parser_Class::element_close(XML_Parser_Class &c,
		   XML_Parser &p,
		   void *data,
		   const char *name)
{
  (void)c; (void)p; (void)name;
  GenXML_Parse_State *state = (GenXML_Parse_State *)data;

  EST_String val;

  
  if (state->depth == state->rel_start_depth )
    {
#ifdef DEBUGGING
      printf("end of relation depth=%d name=%s\n", state->depth, name);
#endif
      state->rel_start_depth=-1;
    }

  if ( 
       state->depth == state->open_depth)
    {
#ifdef DEBUGGING
      printf("pop depth=%d name=%s\n", state->depth, name);
#endif
      state->current = state->parent;
      state->parent=parent(state->parent);
      state->open_depth = state->depth_stack.pop();
#ifdef DEBUGGING
      printf("\t current=%s parent=%s\n", 
	     (const char *)state->current->name(),
	     (const char *)state->parent->name());
#endif
    }


  state->depth--;
}


void GenXML_Parser_Class::pcdata(XML_Parser_Class &c,
	    XML_Parser &p,
	    void *data,
	    const char *chars)
{
  (void)c; 
  (void)p;
 GenXML_Parse_State *state = (GenXML_Parse_State *)data;


 if ( state->parent != NULL && state->contentAttr != EST_String::Empty)
   state->parent->set(state->contentAttr, chars);
 
#ifdef DEBUGGING
 printf("GEN XML Parser [pcdata[%s]] %d\n", chars, state->depth);
#endif
}


void GenXML_Parser_Class::cdata(XML_Parser_Class &c,
	   XML_Parser &p,
	   void *data,
	   const char *chars)
{
  (void)c; (void)p; (void)data; (void)chars;
  // GenXML_Parse_State *state = (GenXML_Parse_State *)data;

#ifdef DEBUGGING
  printf("GEN XML Parser [cdata[%s]] %d\n", chars, state->depth);
#endif
}


void GenXML_Parser_Class::processing(XML_Parser_Class &c,
		XML_Parser &p,
		void *data,
		const char *instruction)
{
  (void)c; (void)p; (void)instruction;
  GenXML_Parse_State *state = (GenXML_Parse_State *)data;
  (void)state;

#ifdef DEBUGGING
  printf("GEN XML Parser [proc[%s]] %d\n", instruction, state->depth);
#endif
}


void GenXML_Parser_Class::error(XML_Parser_Class &c,
	   XML_Parser &p,
	   void *data)
{
  (void)c; (void)p;  (void)data;
  // GenXML_Parse_State *state = (GenXML_Parse_State *)data;

  EST_error("GEN XML Parser %s", get_error(p));

  est_error_throw();
}

template <> EST_String EST_THash<EST_String, EST_Item_Content *>::Dummy_Key = "DUMMY";
template <> EST_Item_Content *EST_THash<EST_String, EST_Item_Content *>::Dummy_Value = NULL;

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_THash.cc"

Instantiate_TStringHash_T(EST_Item_Content *, THash_String_ItemC_P)

#endif
