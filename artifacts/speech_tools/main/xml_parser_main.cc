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
 /* Simple XML parsing mainline.                                          */
 /*                                                                       */
 /*************************************************************************/


#include <cstdlib>
#include <fstream>
#include <iostream>
#include "EST_error.h"
#include "EST_cutils.h"
#include "EST_cmd_line.h"
#include "EST_cmd_line_options.h"
#include "rxp/XML_Parser.h"

struct Parse_State
  {
  int depth;
  };

class My_Parser_Class : public XML_Parser_Class
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

/** @name <command>xml_parser</command>
    @id xml_parser_manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**

   <command>xml_parser</command> is a simple program giving access to
   the <productname>RXP</productname> &xml; parser. It parses an input
   file and prints the results out in a simple indented format. It's
   main use is as a development tool, to check that files intended for
   input to an &xml; application, for instance &festival;, parse as
   you expect.

 */

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}



int main(int argc, char *argv[])
{
  My_Parser_Class pclass;
  EST_Option al;
  EST_StrList files;
  EST_String filename;
  Parse_State state;

  parse_command_line
      (argc, argv, 
	EST_String("[file] [options]\n")+
       "Summary: parse xml and output a tree.\n" 
       "use \"-\" to make input file stdin\n" 
       "-sysdir <string> Look for unqualified system entities in this directory"
       "-h               Options help\n",
			files, al);

  switch (files.length())
    {
    case 0: filename="-";
      break;
    case 1: filename=files.first();
      break;
    default:
      EST_error("Expected only one filename");
      break;
    }

  if (al.present("-sysdir"))
    pclass.register_id("^\\([^/]*\\)",
		       al.sval("-sysdir") + "/\\1");

  pclass.register_id("//CSTR//EST \\(.*\\)",
		     EST_String::cat(est_libdir, "/\\1.dtd"));

  /* An individual parser runs over a single source.
    */
  XML_Parser *parser = pclass.make_parser(filename,
					  &state);
  /* Run the parser.
    */
  parser->go();
  exit(0);
}

static void print_attributes(XML_Attribute_List &attributes)
{
  XML_Attribute_List::Entries them;

  for(them.begin(attributes); them ; them++)
    printf(" %s='%s'", 
	   (const char *)them->k, 
	   (const char *)them->v);
}

/* Now we define the callbacks.
  */

void My_Parser_Class::document_open(XML_Parser_Class &c,
		      XML_Parser &p,
		      void *data)
{
  (void)c; (void)p; 
  (void)print_attributes;
  Parse_State *state = (Parse_State *)data;

  state->depth=1;

  printf("%*s document %d\n", state->depth*4, ">", state->depth);
}

void My_Parser_Class::document_close(XML_Parser_Class &c,
		    XML_Parser &p,
		    void *data)
{
  (void)c; (void)p; 
  Parse_State *state = (Parse_State *)data;

  printf("%*s <document %d\n", state->depth*4, ">", state->depth);
}


void My_Parser_Class::element_open(XML_Parser_Class &c,
		  XML_Parser &p,
		  void *data,
		  const char *name,
		  XML_Attribute_List &attributes)
{
  (void)c; (void)p; (void)attributes;
  Parse_State *state = (Parse_State *)data;

  state->depth++;

  printf("%*s %s %d", state->depth*4, ">", name, state->depth);
  print_attributes(attributes);
  printf("\n");
}


void My_Parser_Class::element(XML_Parser_Class &c,
	     XML_Parser &p,
	     void *data,
	     const char *name,
	     XML_Attribute_List &attributes)
{
  (void)c; (void)p; (void)attributes;
  Parse_State *state = (Parse_State *)data;

  printf("%*s %s %d", state->depth*4, ":", name, state->depth);
  print_attributes(attributes);
  printf("\n");
}


void My_Parser_Class::element_close(XML_Parser_Class &c,
		   XML_Parser &p,
		   void *data,
		   const char *name)
{
  (void)c; (void)p; 
  Parse_State *state = (Parse_State *)data;

  printf("%*s %s %d\n", state->depth*4, "<", name, state->depth);
  state->depth--;
}


void My_Parser_Class::pcdata(XML_Parser_Class &c,
	    XML_Parser &p,
	    void *data,
	    const char *chars)
{
  (void)c; (void)p; 
  Parse_State *state = (Parse_State *)data;

  printf("%*s [pcdata[%s]] %d\n", state->depth*4, "", chars, state->depth);
}


void My_Parser_Class::cdata(XML_Parser_Class &c,
	   XML_Parser &p,
	   void *data,
	   const char *chars)
{
  (void)c; (void)p; 
  Parse_State *state = (Parse_State *)data;

  printf("%*s [cdata[%s]] %d\n", state->depth*4, "", chars, state->depth);
}


void My_Parser_Class::processing(XML_Parser_Class &c,
		XML_Parser &p,
		void *data,
		const char *instruction)
{
  (void)c; (void)p; 
  Parse_State *state = (Parse_State *)data;

  printf("%*s [proc[%s]] %d\n", state->depth*4, "", instruction, state->depth);
}


void My_Parser_Class::error(XML_Parser_Class &c,
	   XML_Parser &p,
	   void *data)
{
  (void)c; (void)p; 
  Parse_State *state = (Parse_State *)data;

  printf("%*s [error[%s]] %d\n", state->depth*4, "", get_error(p), state->depth);
}

