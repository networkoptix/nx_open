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
 /* Simple XML processing example.                                        */
 /*                                                                       */
 /*************************************************************************/

#include <cstdlib>
#include <fstream>
#include <iostream>
#include "rxp/XML_Parser.h"

#if defined(DATAC)
#    define __STRINGIZE(X) #X
#    define DATA __STRINGIZE(DATAC)
#endif

/**@name XML_Parser:example
  * 
  * Simple XML processing example.
  *
  * @see XML_Parser
  */
//@{

/** Any data needed during processing has to be passed around,
  * here we just keep track of the nesting depth.
  */
struct Parse_State
  {
  int depth;
  };

/** Define a subset of XML_Parser_Class which which has callbacks for
  * what we want to do. In a real application we might not need
  * to define them all.
  */
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


int main(void)
{
  My_Parser_Class pclass;
  Parse_State state;

  /** Rewriting rules for public and system IDs can be
    * used to locate local copies etc.
    */

  pclass.register_id("//EST//Test/\\(.*\\)",
		     DATA "/\\1.dtd");

  /** An individual parser runs over a single source.
    */
  XML_Parser *parser = pclass.make_parser(DATA "/eg.xml",
					 &state);
  /** Run the parser.
    */
  parser->go();
  exit(0);
}

/** Now we define the callbacks.
  */

void My_Parser_Class::document_open(XML_Parser_Class &c,
		      XML_Parser &p,
		      void *data)
{
  (void)c; (void)p; 
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

  printf("%*s %s %d\n", state->depth*4, ">", name, state->depth);
}


void My_Parser_Class::element(XML_Parser_Class &c,
	     XML_Parser &p,
	     void *data,
	     const char *name,
	     XML_Attribute_List &attributes)
{
  (void)c; (void)p; (void)attributes;
  Parse_State *state = (Parse_State *)data;

  printf("%*s %s %d\n", state->depth*4, ":", name, state->depth);
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

//@}
