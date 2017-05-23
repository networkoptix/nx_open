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
 /* Functions to talk to siod.                                          */
 /*                                                                       */
 /*************************************************************************/

#include "siod.h"
#include "EST_SiodServer.h"

static bool have_read_table=FALSE;

static int tc_siod_server= -1;

// SIOD support for siod server objects

EST_SiodServer *get_c_siod_server(LISP x)
{
    if (TYPEP(x,tc_siod_server))
	return (EST_SiodServer *)USERVAL(x);
    else
	err("wrong type of argument to get_c_siod_server",x);

    return NULL;  // err doesn't return but compilers don't know that
}

int siod_siod_server_p(LISP x)
{
    if (TYPEP(x,tc_siod_server))
	return TRUE;
    else
	return FALSE;
}

LISP siod_make_siod_server(EST_SiodServer *s)
{
    if (s==0)
	return NIL;
    else
	return siod_make_typed_cell(tc_siod_server,s);
}

static void siod_server_free(LISP lserver)
{
  EST_SiodServer *server = get_c_siod_server(lserver);
  delete server;
  USERVAL(lserver) = NULL;
}

// Deal with the server table.

LISP siod_read_server_table(LISP args)
{
  if (NULLP(args))
    EST_ServiceTable::read_table();
  else
    EST_ServiceTable::read_table(get_c_string(CAR(args)));
  
  have_read_table=TRUE;

  return NIL;
}

LISP siod_write_server_table(LISP args)
{
  if (NULLP(args))
    EST_ServiceTable::write_table();
  else
    EST_ServiceTable::write_table(get_c_string(CAR(args)));
  
  return NIL;
}

LISP siod_servers(void)
{
  EST_StrList names;
  EST_ServiceTable::names(names, "siod");

  if (!have_read_table)
    siod_read_server_table(NIL);
  
  LISP lnames = NIL;
  
  EST_StrList::Entries p;

  for(p.begin(names); p; ++p)
    lnames = cons(strcons((*p).length(), *p), lnames);

  return lnames;
}

// Creating and connecting to servers.

static void siod_connect(EST_SiodServer *server)
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

LISP siod_server(LISP lname)
{
  if (CONSP(lname))
    lname = CAR(lname);

  EST_String name=NULLP(lname)?"siod":get_c_string(lname);
  
  if (!have_read_table)
    siod_read_server_table(NIL);

  LISP verbose = siod_get_lval("siod_verbose", NULL);

  EST_SiodServer *server = 
      new EST_SiodServer(name, !NULLP(verbose)?&cout:(ostream *)NULL);

  siod_connect(server);

  return siod_make_siod_server(server);
}

LISP siod_service_loop(LISP lname)
{
  if (CONSP(lname))
    lname = CAR(lname);

  EST_String name=NULLP(lname)?"siod":get_c_string(lname);
  
  if (!have_read_table)
    siod_read_server_table(NIL);

  LISP verbose = siod_get_lval("siod_verbose", NULL);

  EST_SiodServer *server = new EST_SiodServer(EST_Server::sm_sequential,
					      name, 
					      !NULLP(verbose)?&cout:(ostream *)NULL);

  return siod_make_siod_server(server);
}

static EST_SiodServer *get_server(LISP lserver, bool &my_server)
{
  if (siod_siod_server_p(lserver))
    my_server=false;
  else
    {
      my_server=true;
      lserver = siod_server(lserver);
    }

  return get_c_siod_server(lserver);
}

LISP siod_connect(LISP lserver)
{
  if (!siod_siod_server_p(lserver))
    EST_error("not a siod server %s", get_c_string(lserver));

  EST_SiodServer *server = get_c_siod_server(lserver);

  if (server->connected())
    siod_connect(server);

  return NIL;
}

LISP siod_disconnect(LISP lserver)
{
  if (!siod_siod_server_p(lserver))
    EST_error("not a siod server %s", get_c_string(lserver));

  EST_SiodServer *server = get_c_siod_server(lserver);

  if (server->connected())
    server->disconnect();
  return NIL;
}

LISP siod_server_run(LISP lserver)
{
  if (!siod_siod_server_p(lserver))
    EST_error("not a siod server %s", get_c_string(lserver));

  EST_SiodServer *server = get_c_siod_server(lserver);

  EST_SiodServer::RequestHandler handler;
  
  siod_write_server_table(NIL);

  // Never returns
  server->run(handler);

  return NIL;
}

LISP siod_remote_command(LISP lserver,
			 LISP sexp)
{
  bool my_server;

  EST_SiodServer *server = get_server(lserver, my_server);

  EST_SiodServer::Args args;

  args.set_val("sexp", est_val(sexp));

  EST_SiodServer::ResultHandler handler;
  EST_SiodServer::Result &res = handler.res;

  if (!server->connected())
    siod_connect(server);

  if (!server->execute("scheme", "eval", args, handler))
    {
      EST_String err = res.S("ERROR");
      if (my_server)
	delete server;
      return strcons(err.length(), err);
    }

  if (my_server)
    delete server;

  return scheme(handler.res.val("sexp"));
}

void siod_server_init(void)
{
  long kind;

  tc_siod_server = siod_register_user_type("SiodServer");

  set_gc_hooks(tc_siod_server, 0, NULL,NULL,NULL,siod_server_free,NULL,&kind);


  init_lsubr("siod_read_server_table", siod_read_server_table,
 "(siod_read_server_table &opt FILENAME)\n"
 "   Read the users table of siod servers, or the table\n"
 "   in FILENAME if given.");

  init_lsubr("siod_write_server_table", siod_write_server_table,
 "(siod_write_server_table &opt FILENAME)\n"
 "   Write the users table of siod servers, or the table\n"
 "   in FILENAME if given.");

  init_subr_0("siod_servers", siod_servers,
 "(siod_servers)\n"
 "  Returns a list of the know siod servers. This doesn't\n"
 "  guarantee that they are still running.");

  init_lsubr("siod_server", siod_server,
 "(siod_server &opt NAME)\n"
 "  Return a connection to a siod server with the given name.\n"
 "  If name is omitted it defaults to \"siod\".");

  init_lsubr("siod_service_loop", siod_service_loop,
 "(siod_server_loop &opt NAME)\n"
 "  Return an object representing a network siod server main loop.\n"
 "  This service can be started by calling \\[siod_server_run\\].\n"
 "  If name is omitted it defaults to \"siod\".");

  init_subr_1("siod_connect", siod_connect,
 "(siod_connect SERVER)\n"
 "  Re-open the connection to the server.");

  init_subr_1("siod_disconnect", siod_disconnect,
 "(siod_disconnect SERVER)\n"
 "  Close the connection to the server.");

  init_subr_1("siod_server_run", siod_server_run,
 "(siod_server_run SERVER)\n"
 "  Start the main loop of a siod network server.");

  init_subr_2("siod_remote_command", siod_remote_command,
 "(siod_remote_command SERVER COMMAND)\n"
 "   Evaluate COMMAND on the siod server SERVER.");

}
