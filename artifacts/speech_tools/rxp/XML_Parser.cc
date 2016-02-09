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
 /* Recursive descent parsing skeleton.                                   */
 /*                                                                       */
 /*************************************************************************/

#include "EST_error.h"
#include "XML_Parser.h"
#include "rxp.h"

XML_Parser_Class::XML_Parser_Class()
{
}

void XML_Parser_Class::register_id(EST_Regex id_pattern, EST_String directory)
{
  known_ids.add_item(id_pattern, directory);
}

void XML_Parser_Class::registered_ids(EST_TList<EST_String> &list)
{
  EST_Litem *p;

  for(p=known_ids.head(); p != 0; p= p->next())
    {
      EST_String re(known_ids.key(p).tostring());
      EST_String &pattern = known_ids.val(p);

      list.append(re);
      list.append(pattern);
    }
}

XML_Parser *XML_Parser_Class::make_parser(InputSource source, Entity ent, void *data)
{
  return new XML_Parser(*this, source, ent, data);
}

XML_Parser *XML_Parser_Class::make_parser(InputSource source, void *data)
{
  return new XML_Parser(*this, source, NULL, data);
}


XML_Parser *XML_Parser_Class::make_parser(FILE *input, 
					   const EST_String desc, 
					   void *data)
{
  Entity ent = NewExternalEntity(0,0,strdup8(desc),0,0);

  FILE16 *input16=MakeFILE16FromFILE(input, "r");

  if (input16==NULL)
    EST_sys_error("Can't open 16 bit '%s'", (const char *)desc);

  SetCloseUnderlying(input16, 0);

  return make_parser(NewInputSource(ent, input16), ent, data);
}


XML_Parser *XML_Parser_Class::make_parser(FILE *input, 
					   void *data)
{
  return make_parser(input, "<ANONYMOUS>", data);
}


XML_Parser *XML_Parser_Class::make_parser(const EST_String filename, 
					   void *data)
{
  if ( filename == "-" )
    return make_parser(stdin, data);

  FILE *input = fopen(filename, "r");

  if (input==NULL)
    EST_sys_error("Can't open '%s'", (const char *)filename);

  Entity ent = NewExternalEntity(0,0,strdup8(filename),0,0);

  FILE16 *input16=MakeFILE16FromFILE(input, "r");

  if (input16==NULL)
    EST_sys_error("Can't open 16 bit '%s'", (const char *)filename);

  SetCloseUnderlying(input16, 1);

  return make_parser(NewInputSource(ent, input16), data);
}

InputSource XML_Parser_Class::try_and_open(Entity ent)

{
  EST_String id = ent->publicid?ent->publicid:ent->systemid;
  EST_Litem *p;

  int starts[EST_Regex_max_subexpressions];
  int ends[EST_Regex_max_subexpressions];
  for (p = known_ids.head(); p != 0; p = p->next())
    {
      EST_Regex &re = known_ids.key(p);
      EST_String pattern(known_ids.val(p));

      if (id.matches(re, 0, starts, ends))
	{
	  EST_String res(pattern);
	  res.subst(id, starts, ends);

	  FILE *f;
	  FILE16 *f16;
	  if((f = fopen(res, "r")))
	    {
	      if(!(f16 = MakeFILE16FromFILE(f, "r")))
		return 0;
	      SetCloseUnderlying(f16, 1);
	      
	      return NewInputSource(ent, f16);
	    }
	}
    }
  
  return EntityOpen(ent);
}


InputSource XML_Parser_Class::open_entity(Entity ent, void *arg)
{
  XML_Parser *parser = (XML_Parser *)arg;

  return parser->open(ent);
}

// Default do-nothing callbacks.

void XML_Parser_Class::document_open(XML_Parser_Class &c,
			XML_Parser &p,
			void *data)
{ (void)c; (void)p; (void)data; }

void XML_Parser_Class::document_close(XML_Parser_Class &c,
				      XML_Parser &p,
				      void *data)
{ (void)c; (void)p; (void)data; }
  
void XML_Parser_Class::element_open(XML_Parser_Class &c,
		       XML_Parser &p,
		       void *data,
		       const char *name,
		       XML_Attribute_List &attributes)
{ (void)c; (void)p; (void)data; (void)name; (void)attributes; }

void XML_Parser_Class::element(XML_Parser_Class &c,
		  XML_Parser &p,
		  void *data,
		  const char *name,
		  XML_Attribute_List &attributes)
{ (void)c; (void)p; (void)data; (void)name; (void)attributes; 
  element_open(c, p, data, name, attributes);
  element_close(c, p, data, name);
}

void XML_Parser_Class::element_close(XML_Parser_Class &c,
			XML_Parser &p,
			void *data,
			const char *name)
{ (void)c; (void)p; (void)data; (void)name; }

void XML_Parser_Class::pcdata(XML_Parser_Class &c,
		 XML_Parser &p,
		 void *data,
		 const char *chars)
{ (void)c; (void)p; (void)data; (void)chars; }

void XML_Parser_Class::cdata(XML_Parser_Class &c,
		XML_Parser &p,
		void *data,
		const char *chars)
{ (void)c; (void)p; (void)data; (void)chars; }

void XML_Parser_Class::processing(XML_Parser_Class &c,
		     XML_Parser &p,
		     void *data,
		     const char *instruction)
{ (void)c; (void)p; (void)data; (void)instruction; }

void XML_Parser_Class::error(XML_Parser_Class &c,
		XML_Parser &p,
		void *data)
{ (void)c; (void)p; (void)data; }

const char *XML_Parser_Class::get_error(XML_Parser &p)
{
  return p.get_error();
}

void XML_Parser_Class::error(XML_Parser_Class &c,
		XML_Parser &p,
		void *data,
		EST_String message)
{
  if (p.current_bit != NULL)
    p.current_bit->error_message = message;
  error(c, p, data);
}

 /*************************************************************************/
 /*                                                                       */
 /* An actual parser.                                                     */
 /*                                                                       */
 /*************************************************************************/

XML_Parser::XML_Parser(XML_Parser_Class &pc, 
		       InputSource s,
		       Entity ent,
		       void *d)
{
  pclass=&pc;
  source=s;
  initial_entity=ent;
  data=d;
  p = NewParser();
  ParserSetEntityOpener(p, XML_Parser_Class::open_entity);
  ParserSetFlag(p, ReturnDefaultedAttributes, 1);
  ParserSetCallbackArg(p, (void *)this);
}

XML_Parser::~XML_Parser()
{
  if (initial_entity) 
    FreeEntity(initial_entity);
  FreeDtd(p->dtd);
  FreeParser(p);
}

InputSource XML_Parser::open(Entity ent)
{
  return pclass->try_and_open(ent);
}

void XML_Parser::go()
{

  if (p_track_context)
    p_context.clear();

  if (ParserPush(p, source) == -1)
      EST_error("XML Parser error in push");

  pclass->document_open(*pclass, *this, data);

  XBit bit;
  while (1)
    {
        current_bit = bit = ReadXBit(p);
	if (bit->type == XBIT_eof)
	    break;
	else if (bit->type == XBIT_start || bit->type ==  XBIT_empty)
	{
	    Attribute b;
	    XML_Attribute_List att(10);

	    for (b=bit->attributes; b; b=b->next)
	      {
		att.add_item(EST_String(b->definition->name), EST_String(b->value));
	      }

	    if (bit->type == XBIT_start)
	      {
		pclass->element_open(*pclass, 
				     *this, 
				     data,
				     bit->element_definition->name,
				     att
				     );
		if (p_track_context)
		  {
		    EST_String nm(bit->element_definition->name);
		    p_context.push(nm);
		  }
		   
	      }
	      else
		pclass->element(*pclass, 
				   *this, 
				   data,
				   bit->element_definition->name,
				   att
				   );
	}
	else if (bit->type == XBIT_end)
	{
	  if (p_track_context)
	    p_context.pop();

	  pclass->element_close(*pclass, 
				*this, 
				data,
				bit->element_definition->name
				);
	}
	else if (bit->type == XBIT_pcdata)
	{
	  pclass->pcdata(*pclass, 
			 *this, 
			 data,
			 bit->pcdata_chars
			 );
	}
	else if (bit->type == XBIT_cdsect)
	{
	  pclass->cdata(*pclass, 
			*this, 
			data,
			bit->cdsect_chars
			);
	}
	else if (bit->type == XBIT_pi)
	{
	  pclass->processing(*pclass, 
			     *this, 
			     data,
			     bit->pi_chars
			     );
	}
	else if (bit->type == XBIT_error)
	{
	  pclass->error(*pclass, 
			*this, 
			data);
	  break;
	}
	else
	{
	    // ignore it
	}
	FreeXBit(bit);
	current_bit=NULL;
    }

  if (current_bit!=NULL)
    {
      FreeXBit(bit);
      current_bit=NULL;
    }

  pclass->document_close(*pclass, *this, data);
}

void XML_Parser::track_context(bool flag)
{
  p_track_context=flag;
}

void XML_Parser::track_contents(bool flag)
{
  p_track_contents=flag;
}


// Stolen from xmlparser.c, will need to be tweaked for internal rxp changes.
const char *XML_Parser::get_error()
{
  int linenum, charnum;
  InputSource s;
  XBit bit = current_bit;

  if (!bit)
    return "No Parse In Progress";

  p_error_message = 
    EST_String::cat(
		    bit->type == XBIT_error ? "Error" : "Warning",
		    ": ",
		    bit->error_message?bit->error_message:"non XML error"
		    );

  for(s=p->source; s; s=s->parent)
    {
	if(s->entity->name)
	  {
	    p_error_message += " in entity \"";
	    p_error_message += s->entity->name;
	    p_error_message += "\"";
	  }
	else
	  p_error_message += " in unnamed entity";

	switch(SourceLineAndChar(s, &linenum, &charnum))
	{
	case 1:
	  p_error_message += EST_String::cat(" at line ",
					     EST_String::Number(linenum+1),
					     " char ",
					     EST_String::Number(charnum+1),
					     " of ");
	  break;
	case 0:
	  p_error_message += EST_String::cat(" defined at line ",
					     EST_String::Number(linenum+1),
					     " char ",
					     EST_String::Number(charnum+1),
					     " of ");
	  break;
	case -1:
	  p_error_message += " defined in ";
	  break;
	}

	p_error_message += EntityDescription(s->entity);
	p_error_message += "\n";
    }

  return (const char *)p_error_message;
}

EST_String XML_Parser::context(int n)
{
  return p_context.nth(n);
}

