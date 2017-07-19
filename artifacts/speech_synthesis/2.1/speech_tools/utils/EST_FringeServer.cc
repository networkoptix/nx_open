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
 /* Code to talk to a running fringe.                                     */
 /*                                                                       */
 /*************************************************************************/

#include "EST_system.h"
#include "EST_socket.h"
#include <csignal>
#include "EST_unix.h"
#include "EST_TKVL.h"
#include "EST_ServiceTable.h"
#include "EST_FringeServer.h"
#include "EST_Pathname.h"
#include "EST_error.h"
#include "EST_Token.h"
#include <iomanip>
#include <iostream>

static EST_Regex ipnum("[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+");


EST_FringeServer::ResultHandler::ResultHandler()
{
}

EST_FringeServer::ResultHandler::~ResultHandler()
{
}

void EST_FringeServer::ResultHandler::process(void)
{
}

EST_FringeServer::EST_FringeServer(EST_String name, ostream *trace)
   : EST_Server(name, "fringe", trace)
{
}

EST_FringeServer::EST_FringeServer(EST_String name)
   : EST_Server(name, "fringe")
{
}

EST_FringeServer::EST_FringeServer(EST_String hostname, int port, ostream *trace)
   : EST_Server(hostname, port, trace)
{
}

EST_FringeServer::EST_FringeServer(EST_String hostname, int port)
   : EST_Server(hostname, port)
{
}

EST_FringeServer::~EST_FringeServer()
{
}

static void setup_command_tokenstream(EST_TokenStream &toks)
{
  toks.set_SingleCharSymbols("(){}[]=");
  toks.set_PunctuationSymbols("");
  toks.set_PrePunctuationSymbols("");
  toks.set_quotes('"', '\\');
}

bool EST_FringeServer::parse_command(const EST_String command,
				     EST_String &package,
				     EST_String &operation,
				     EST_FringeServer::Args &arguments)
{
  EST_TokenStream toks;
  EST_Token tok;
  int i;

  toks.open_string(command);

  setup_command_tokenstream(toks);

  if (toks.eof())
    return FALSE;

  tok=toks.get();

  EST_String op = tok.String();
  if ((i = op.index(".", -1)) >= 0)
    {
      package = op.before(i,1);
      operation = op.after(i,1);
    }
  else
    {
      package = "";
      operation = op;
    }

  while (!toks.eof())
    {
      tok=toks.get();

      if (tok.String() == "")
	break;

      EST_String key=tok.String();
      EST_String val;

      tok = toks.peek();
      if (tok.String() == "=")
	{
	  // swallow '='
	  toks.get();

	  if (toks.eof())
	    return FALSE;

	  tok=toks.get();

	  if (tok.String() == "")
	    return FALSE;

	  val=tok.String();
	}
      else
	{
	  val=key;
	  key="";
	}

      arguments.set(key,val);
    }


  toks.close();
  return TRUE;
}

EST_String  EST_FringeServer::build_command(const EST_String package,
					    const EST_String operation,
					    const EST_Server::Args &arguments)
{
  EST_String c="";

  if (package != "")
    c += package + ".";

  c += operation;

  EST_FringeServer::Args::Entries argp;
		
  for (argp.begin(arguments); argp != 0; ++argp)
    {
      c += " ";
      if (argp->k != "")
	c += argp->k + "=";
      
      c += argp->v.String().quote_if_needed('"');
    }

  return c;
}



bool EST_FringeServer::parse_result(const EST_String resultString,
				    EST_Server::Result &res)
{
  res.set("STRING", resultString);
  return TRUE;
}

EST_String EST_FringeServer::build_result(const EST_Server::Result &res)
{
  (void)res;
  return "";
}
