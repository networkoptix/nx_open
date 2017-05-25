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
 /* Functions to talk to fringe.                                          */
 /*                                                                       */
 /*************************************************************************/

#include "siod.h"
#include "EST_FringeServer.h"

static bool have_read_table=FALSE;

static int tc_fringe_server= -1;

// SIOD support for fringe server objects

EST_FringeServer *get_c_fringe_server(LISP x)
{
    if (TYPEP(x,tc_fringe_server))
	return (EST_FringeServer *)USERVAL(x);
    else
	err("wrong type of argument to get_c_fringe_server",x);

    return NULL;  // err doesn't return but compilers don't know that
}

int siod_fringe_server_p(LISP x)
{
    if (TYPEP(x,tc_fringe_server))
	return TRUE;
    else
	return FALSE;
}

LISP siod_make_fringe_server(EST_FringeServer *s)
{
    if (s==0)
	return NIL;
    else
	return siod_make_typed_cell(tc_fringe_server,s);
}

static void fringe_server_free(LISP lserver)
{
  EST_FringeServer *server = get_c_fringe_server(lserver);
  delete server;
  USERVAL(lserver) = NULL;
}

// Deal with the server table.

LISP fringe_read_server_table(LISP args)
{
  if (NULLP(args))
    EST_ServiceTable::read_table();
  else
    EST_ServiceTable::read_table(get_c_string(CAR(args)));
  
  have_read_table=TRUE;

  return NIL;
}

LISP fringe_servers(void)
{
  EST_StrList names;
  EST_ServiceTable::names(names);

  if (!have_read_table)
    fringe_read_server_table(NIL);
  
  LISP lnames = NIL;
  
  EST_StrList::Entries p;

  for(p.begin(names); p; ++p)
    lnames = cons(strcons((*p).length(), *p), lnames);

  return lnames;
}

// Creating and connecting to servers.

static void fringe_connect(EST_FringeServer *server)
{
  switch (server->connect())
    {
    case connect_ok:
      break;

    case connect_not_found_error:
      EST_sys_error("Can't find host '%s:%d'", (const char *)server->servername(), server->port());
      break;
      
    case connect_not_allowed_error:
      EST_sys_error("Can't connect to '%s:%d'", (const char *)server->servername(), server->port());
      break;

    default:
      EST_sys_error("Error connecting to  '%s:%d'", (const char *)server->servername(), server->port());
      break;
    }
}

LISP fringe_server(LISP lname)
{
  if (CONSP(lname))
    lname = CAR(lname);

  EST_String name=NULLP(lname)?"fringe":get_c_string(lname);
  
  if (!have_read_table)
    fringe_read_server_table(NIL);

  LISP verbose = siod_get_lval("fringe_verbose", NULL);

  EST_FringeServer *server = new EST_FringeServer(name, !NULLP(verbose)?&cout:(ostream *)NULL);

  fringe_connect(server);

  return siod_make_fringe_server(server);
}

static EST_FringeServer *get_server(LISP lserver, bool &my_server)
{
  if (siod_fringe_server_p(lserver))
    my_server=false;
  else
    {
      my_server=true;
      lserver = fringe_server(lserver);
    }

  return get_c_fringe_server(lserver);
}

LISP fringe_connect(LISP lserver)
{
  if (!siod_fringe_server_p(lserver))
    EST_error("not a fringe server %s", get_c_string(lserver));

  EST_FringeServer *server = get_c_fringe_server(lserver);

  if (server->connected())
    fringe_connect(server);

  return NIL;
}

LISP fringe_disconnect(LISP lserver)
{
  if (!siod_fringe_server_p(lserver))
    EST_error("not a fringe server %s", get_c_string(lserver));

  EST_FringeServer *server = get_c_fringe_server(lserver);

  if (server->connected())
    server->disconnect();
  return NIL;
}

LISP fringe_command_string(LISP lserver,
			   LISP lcommand)
{
  EST_String command=get_c_string(lcommand);

  bool my_server;

  EST_FringeServer *server = get_server(lserver, my_server);

  if (!server->connected())
    fringe_connect(server);

  EST_FringeServer::ResultHandler handler;

  EST_FringeServer::Result &res = handler.res;

  if (!server->execute(command, handler))
    {
      EST_String err = res.S("ERROR");
      if (my_server)
	delete server;
      return strcons(err.length(), err);
    }

  if (my_server)
    delete server;

  return NIL;
}

LISP fringe_command(LISP lserver,
		    LISP lpackage,
		    LISP loperation,
		    LISP largs)
{
  bool my_server;

  EST_FringeServer *server = get_server(lserver, my_server);

  EST_String package = NULLP(lpackage)?"":get_c_string(lpackage);
  EST_String operation = NULLP(loperation)?"":get_c_string(loperation);

  EST_FringeServer::Args args;

  if (!LISTP(largs))
    EST_error("Bad argument list");

  lisp_to_features(largs, args);

  EST_FringeServer::ResultHandler handler;
  EST_FringeServer::Result &res = handler.res;

  if (!server->connected())
    fringe_connect(server);

  if (!server->execute(package, operation, args, handler))
    {
      EST_String err = res.S("ERROR");
      if (my_server)
	delete server;
      return strcons(err.length(), err);
    }

  if (my_server)
    delete server;

  return NIL;
}

void siod_fringe_init()
{
  long kind;

  tc_fringe_server = siod_register_user_type("FringeServer");
  set_gc_hooks(tc_fringe_server, 0, NULL,NULL,NULL,fringe_server_free,NULL,&kind);


  init_lsubr("fringe_read_server_table", fringe_read_server_table,
 "(fringe_read_server_table &opt FILENAME)\n"
 "   Read the users table of fringe servers, or the table\n"
 "   in FILENAME if given.");

  init_subr_0("fringe_servers", fringe_servers,
 "(fringe_servers)\n"
 "  Returns a list of the know fringe servers. This doesn't\n"
 "  guarantee that they are still running.");

  init_lsubr("fringe_server", fringe_server,
 "(fringe_server &opt NAME)\n"
 "  Return a connection to a fringe server with the given name.\n"
 "  If name is omitted it defaults to \"fringe\".");

  init_subr_1("fringe_disconnect", fringe_disconnect,
 "(fringe_disconnect SERVER)\n"
 "  Close the connection to the server.");

  init_subr_1("fringe_connect", fringe_connect,
 "(fringe_connect SERVER)\n"
 "  Re-open the connection to the server.");

  init_subr_2("fringe_command_string", fringe_command_string,
 "(fringe_command_string SERVER COMMAND)\n"
 "   Send COMMAND to the fringe server SERVER.");

  init_subr_4("fringe_command", fringe_command,
 "(fringe_command SERVER PACKAGE OPERATION ARGS) \n"
 "   Send command to the fringe server SERVER.\n"
 "   ARGS should be an association list of key-value pairs.");
}
