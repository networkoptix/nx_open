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
 /* Simple scheme server.                                                 */
 /*                                                                       */
 /*************************************************************************/

#include "EST_system.h"
#include "EST_unix.h"
#include "EST_ServiceTable.h"
#include "EST_SiodServer.h"
#include "EST_Pathname.h"
#include "EST_error.h"
#include "EST_Token.h"
#include "siod.h"

static EST_Regex ipnum("[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+");


EST_SiodServer::ResultHandler::ResultHandler()
{
}

EST_SiodServer::ResultHandler::~ResultHandler()
{
}

void EST_SiodServer::ResultHandler::process(void)
{
}

EST_SiodServer::RequestHandler::RequestHandler()
{
}

EST_SiodServer::RequestHandler::~RequestHandler()
{
}

EST_String EST_SiodServer::RequestHandler::process(void)
{
  volatile LISP sexp = scheme(args.val("sexp"));
  volatile LISP result = NULL;

  CATCH_ERRORS()
    {
      res.set_val("sexp", est_val(NIL));
      return "siod error";
    }

  result = leval(sexp, current_env);

  END_CATCH_ERRORS();

  res.set_val("sexp", est_val(result));

  return "";
}

EST_SiodServer::EST_SiodServer(EST_String name, ostream *trace)
   : EST_Server(name, "siod", trace)
{
}

EST_SiodServer::EST_SiodServer(EST_String name)
   : EST_Server(name, "siod")
{
}

EST_SiodServer::EST_SiodServer(EST_String hostname, int port, ostream *trace)
   : EST_Server(hostname, port, trace)
{
}

EST_SiodServer::EST_SiodServer(EST_Server::Mode mode, EST_String name)
  : EST_Server(mode, name, "siod")
{
}

EST_SiodServer::EST_SiodServer(EST_Server::Mode mode, EST_String name, ostream *trace)
  : EST_Server(mode, name, "siod", trace)
{
}

EST_SiodServer::EST_SiodServer(EST_String hostname, int port)
   : EST_Server(hostname, port)
{
}

EST_SiodServer::~EST_SiodServer()
{
}

bool EST_SiodServer::parse_command(const EST_String command,
				   EST_String &package,
				   EST_String &operation,
				   EST_SiodServer::Args &arguments)
{
  package="scheme";
  operation="eval";

  LISP sexp = read_from_string((char *)(const char *)command);

  arguments.set_val("sexp", est_val(sexp));

  return TRUE;
}

EST_String  EST_SiodServer::build_command(const EST_String package,
					  const EST_String operation,
					  const EST_SiodServer::Args &arguments)
{
    (void)operation;
  if (package == "scheme" && operation == "eval")
    {
	LISP sexp = scheme(arguments.val("sexp"));
	return siod_sprint(sexp);
    }
    else
	return "";
}

bool EST_SiodServer::parse_result(const EST_String resultString,
				  EST_Server::Result &res)
{
  LISP sexp = read_from_string((char *)(const char *)resultString);

  res.set_val("sexp", est_val(sexp));
  return TRUE;
}

EST_String EST_SiodServer::build_result(const EST_Server::Result &res)
{
  
  LISP sexp = scheme(res.val("sexp"));
  return siod_sprint(sexp);
}



