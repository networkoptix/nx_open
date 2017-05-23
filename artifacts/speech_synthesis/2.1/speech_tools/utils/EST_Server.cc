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
 /* Code to talk to a running server.                                     */
 /*                                                                       */
 /*************************************************************************/

#include "EST_system.h"
#include "EST_socket.h"
#include <csignal>
#include "EST_unix.h"
#include "EST_TKVL.h"
#include "EST_ServiceTable.h"
#include "EST_Server.h"
#include "EST_Pathname.h"
#include "EST_error.h"
#include "EST_Token.h"
#include <iomanip>
#include <iostream>

static EST_Regex ipnum("[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+");

static EST_String cookie_term = "\n";

static EST_String command_term = "\n//End//\n";

static EST_String result_term = "\n//End//";

static EST_String status_term = "\n";

EST_Server::RequestHandler::RequestHandler()
{
}

EST_Server::RequestHandler::~RequestHandler()
{
}

EST_Server::ResultHandler::ResultHandler()
{
}

EST_Server::ResultHandler::~ResultHandler()
{
}

EST_Server::BufferedSocket::BufferedSocket(int socket)
{
  s = socket;
  bpos=0;
  blen=100;
  buffer=walloc(char, 100);
}

EST_Server::BufferedSocket::~BufferedSocket()
{
  if (buffer != NULL)
    {
     wfree(buffer);
     buffer=NULL;
    }
}

void EST_Server::BufferedSocket::ensure(int n)
{
  if (n > blen)
    {
      buffer= wrealloc(buffer, char, n+100);
      blen = n+100;
    }
}

int EST_Server::BufferedSocket::read_data(void)
{
return  ::read(s, buffer+bpos, blen-bpos);
}

EST_Server::EST_Server(Mode mode, EST_String name, EST_String type, ostream *trace)
{
  zero();
  if (mode == sm_client)
    initClient(name, type, trace);
  else
    initServer(mode, name, type, trace);
}

EST_Server::EST_Server(EST_String name, EST_String type, ostream *trace)
{
   zero();
   initClient(name, type, trace);
}

EST_Server::EST_Server(Mode mode, EST_String name, EST_String type) 
{
  zero();
  if (mode == sm_client)
    initClient(name, type, NULL);
  else
    initServer(mode, name, type, NULL);
}

EST_Server::EST_Server(EST_String name, EST_String type)
{
  zero();
  initClient(name, type, NULL);
}

EST_Server::EST_Server(EST_String hostname, int port, ostream *trace)
{
  zero();
  initClient(hostname, port, trace);
}

EST_Server::EST_Server(EST_String hostname, int port)
{
  zero();
  initClient(hostname, port, NULL);
}

EST_Server::~EST_Server()
{
  if (connected())
    disconnect();
//  if (p_serv_addr)
//    delete p_serv_addr;
  if (p_buffered_socket)
    delete p_buffered_socket;
}

void EST_Server::zero()
{
  p_serv_addr = NULL;
  p_socket = -1;
  p_mode = sm_unknown;
  p_trace = NULL;
}

void EST_Server::initClient(EST_String name, EST_String type, ostream *trace)
{
  const EST_ServiceTable::Entry &entry = EST_ServiceTable::lookup(name, type);

  //  if (&entry == EST_ServiceTable::EntryTable::default_val)
  if (entry.port == 0)
    EST_error("no %s called '%s' listed",
	      (const char *)type, (const char *)name);

  initClient(entry, trace);
}

void EST_Server::initClient(EST_String hostname, int port, ostream *trace)
{
  EST_ServiceTable::Entry tmp;

  tmp.name="<UNKNOWN>";
  if (hostname.matches(ipnum))
    {
      tmp.hostname="";
      tmp.address=hostname;
    }
  else
    {
      tmp.address="";
      tmp.hostname=hostname;
    }
  tmp.port=port;
  initClient(tmp, trace);
}

void EST_Server::init(ostream *trace)
{

  p_trace=trace;

  if (!socket_initialise())
    EST_sys_error("Can't Initialise Network Code");

  if (connected())
    disconnect();

    if (p_serv_addr != NULL)
      {
//	delete p_serv_addr;
	p_serv_addr = NULL;
      }
}

void EST_Server::initClient(const EST_ServiceTable::Entry &entry, ostream *trace)
{
  init(trace);

  p_mode = sm_client;
  p_entry = entry;

  struct sockaddr_in *serv_addr = new sockaddr_in;

  p_serv_addr = serv_addr;
  struct hostent *serverhost;
  
  memset(serv_addr, 0, sizeof(serv_addr));

  if (p_entry.address != "")
    {
      if (p_trace != NULL)
	*p_trace << "Using address "
		 << entry.address
		 << "\n";

      serv_addr->sin_addr.s_addr = inet_addr(entry.address);
    }
  else
    {
      if (p_trace != NULL)
	*p_trace << "Using domain name "
		 << entry.hostname
		 << "\n";

      serverhost = gethostbyname(entry.hostname);

      if (serverhost == (struct hostent *)0)
	EST_error("lookup of host '%s' failed", (const char *)entry.hostname);
		  
      memmove(&(serv_addr->sin_addr),serverhost->h_addr, serverhost->h_length);
    }

  if (p_trace != NULL)
    *p_trace << "Server is at "
	     << inet_ntoa(serv_addr->sin_addr)
		 << "\n";

  serv_addr->sin_family = AF_INET;
  serv_addr->sin_port = htons(entry.port);

}

void EST_Server::initServer(Mode mode, EST_String name, EST_String type, ostream *trace)
{
  init(trace);
  p_mode = mode;

  int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (NOT_A_SOCKET(s))
    EST_sys_error("Can't create socket");

  struct sockaddr_in serv_addr;

  memset(&serv_addr, 0, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(0);
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);


  if (bind(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
    EST_sys_error("bind failed");
  
  if (listen(s, 5) != 0)
    EST_sys_error("listen failed");

  p_entry = EST_ServiceTable::create(name, type, s);
  p_socket = s;
}


const EST_String EST_Server::name(void) const
{ return p_entry.name; }

const EST_String EST_Server::type(void) const
{ return p_entry.type; }

const EST_String EST_Server::hostname(void) const
{ return p_entry.hostname; }

const EST_String EST_Server::address(void) const
{ return p_entry.address; }

const EST_String EST_Server::servername(void) const
{
  if (p_entry.hostname != "")
    return p_entry.hostname;
  else if (p_entry.address != "")
    return p_entry.address;

  return "<UNKNOWN HOST>";
}

int EST_Server::port(void) const
{ return p_entry.port; }

EST_connect_status EST_Server::connect(void)
{
  if (connected())
    EST_error("Already Connected");

  if (p_mode != sm_client)
    EST_error("Connect is not a legal action on server end connections");

  struct sockaddr_in *serv_addr = (struct sockaddr_in *) p_serv_addr;

  p_socket = socket(serv_addr->sin_family, SOCK_STREAM, IPPROTO_TCP);

  if (NOT_A_SOCKET(p_socket))
    return connect_system_error;

  if (p_trace != NULL)
    *p_trace << "Connect to "
	     << inet_ntoa(serv_addr->sin_addr)
	     << ":"
	     << ntohs(serv_addr->sin_port)
	     << "\n";

  if (::connect(p_socket, (struct sockaddr *)serv_addr, sizeof(*serv_addr)) != 0)
    {
      int t_errno=errno;	// save this so it isn't changed by close()
      close(p_socket);
      p_socket=-1;
      errno=t_errno;
      if (
#if !defined(SYSTEM_IS_WIN32)
	  t_errno == ECONNREFUSED ||
#endif
	  t_errno == EACCES
	  )
	return connect_not_allowed_error;
      if (
#if !defined(SYSTEM_IS_WIN32)
	  t_errno == ENETUNREACH ||
	  t_errno == EADDRNOTAVAIL ||
#endif
	  t_errno == ENOENT
	  )
      return connect_not_found_error;
    }

  p_buffered_socket = new BufferedSocket(p_socket);

  if (p_entry.cookie != "" && p_entry.cookie != "none")
    write(*p_buffered_socket, "//"+p_entry.cookie, cookie_term);

  return connect_ok;
}

bool EST_Server::connected(void)
{
  return p_socket >=0;
}

EST_connect_status EST_Server::disconnect(void)
{
  if (!connected())
    EST_error("Not Connected");

  if (p_trace)
  {
    if (p_mode == sm_client)
      *p_trace << "Disconnect from  "
	       << p_entry.name
	       << "\n";
    else
      *p_trace << "Close down service "
	       << p_entry.name
	       << "\n";
  }

  if (p_buffered_socket)
    {
      delete p_buffered_socket;
      p_buffered_socket = NULL;
    }

  close(p_socket);
  p_socket=-1;

  return connect_ok;
}

bool EST_Server::parse_command(const EST_String command,
			       EST_String &package,
			       EST_String &operation,
			       EST_Server::Args &arguments)
{
  (void)command;
  (void)package;
  (void)operation;
  (void)arguments;
  return false;
}

EST_String EST_Server::build_command(const EST_String package,
				     const EST_String operation,
				     const EST_Server::Args &arguments)
{
  (void)package;
  (void)operation;
  (void)arguments;
  return "";
}

bool EST_Server::parse_result(const EST_String resultString,
			      EST_Server::Result &res)
{
  res.set("STRING", resultString);
  return false;
}

EST_String EST_Server::build_result(const EST_Server::Result &res)
{
  (void)res;
  return "";
}
				   


bool EST_Server::execute(const EST_String package,
			 const EST_String operation,
			 const EST_Server::Args &args,
			 EST_Server::ResultHandler &handler)
{

  EST_String command = build_command(package, operation, args);

  return execute(command, handler);
}

static int server_died;
static void server_died_fn(int x)
{
  (void)x;
  server_died=1;
}

void EST_Server::write(BufferedSocket &s, const EST_String string, const EST_String term)
{
  EST_String termed;

  const char *data;
  int l, p=0, n;

  if (term != "")
    {
      termed = EST_String::cat(string, term);
      data = termed;
      l = termed.length();
    }
  else
    {
      data = string;
      l = string.length();
    }


  server_died=0;
#if !defined(SYSTEM_IS_WIN32)
  void (*old)(int) = signal(SIGPIPE, server_died_fn);
#endif

  while(l>0)
    if ((n=::write(s.s, data+p, l))>0)
      {
	 if (p_trace)
	   *p_trace << "wrote " << n << " chars\n";
	l -= n;
	p += n;
      }
  else
    {
#if !defined(SYSTEM_IS_WIN32)
      signal(SIGPIPE, old);
#endif
      EST_sys_error("Failed while writing to %s", (const char *)p_entry.type);
    }

#if !defined(SYSTEM_IS_WIN32)
  signal(SIGPIPE, old);
#endif

  if (server_died)
    EST_error("Failed while writing to %s", (const char *)p_entry.type);
}

EST_String EST_Server::read_data(BufferedSocket &sock, const EST_String end, int &eof)
{
  EST_String result(sock.buffer, 0, sock.bpos);
  int endpos;

  eof=0;
  sock.bpos=0;
  
  while ((endpos=result.index(end, 0))<0 && 1==1)
    {
      int n=sock.read_data();

      if ( n== 0 )
	{
	  eof=1;
	  return "EOF";
	}

      if (n<0)
	EST_sys_error("Failed to read from %s %d", (const char *)p_entry.type, errno);

      EST_String bit(sock.buffer, 0, n+sock.bpos);

      if (p_trace)
	*p_trace << "read " << n << " chars '" << bit << "'\n";

      result += bit;

      sock.bpos=0;
    }

  sock.ensure(result.length()-endpos);

  memcpy(sock.buffer, 
	 (const char *)result+endpos+end.length(), 
	 result.length()-endpos-end.length());

  sock.bpos=result.length()-endpos-end.length();

  result=result.at(0, endpos);

  if (p_trace)
    *p_trace << "Got '"
	     << result
	     << "'\n"
	     << "Left '"
	     << EST_String(sock.buffer, 0, sock.bpos)
      	     << "'\n";

  return result;
}

bool EST_Server::execute(const EST_String command,
			 EST_Server::ResultHandler &handler)
{
  if (!connected())
    EST_error("Must connect to %s before calling execute.", (const char *)p_entry.type);

  handler.server=this;
  Result &res = handler.res;

  if (p_trace)
    *p_trace << "Sending command "
	     << command
	     << " to "
	     << p_entry.type
	     << "\n";

  write(*p_buffered_socket, command, command_term);

  int eof;

  EST_String result = read_data(*p_buffered_socket, result_term, eof);
  if (eof)
    {
      res.set("ERROR", "server closed connection");
      handler.resString=result;
      handler.process();
      return FALSE;
    }

  EST_String status = read_data(*p_buffered_socket, status_term, eof);
  if (eof)
    {
      res.set("ERROR", "server closed connection");
      handler.resString=result;
      handler.process();
      return FALSE;
    }


  if (status == "ERROR")
    {
      res.set("ERROR", result);
      handler.resString=result;
      handler.process();
      return FALSE;
    }
  else if (!parse_result(result, res))
    {
      res.set("ERROR", "Server returned bad result '"+result+"'");
      handler.resString=result;
      handler.process();
      return FALSE;
    }
    
  handler.resString=result;
  handler.process();
  return TRUE;
}

void EST_Server::run(EST_Server::RequestHandler &handler)
{
  if (!connected())
    EST_error("Server disconnected", (const char *)p_entry.type);

  handler.server=this;

  switch(p_mode)
    {
    case sm_sequential:
      run_sequential(handler);
      break;

    default:
      EST_error("Server type %d not yet implemented", (int)p_mode);
      break;
    }

}

void EST_Server::run_sequential(EST_Server::RequestHandler &handler)
{
  int csocket=0;
  struct sockaddr_in sin;
  socklen_t sin_size = sizeof(sin);

  while (connected() && 
	 (csocket = accept(p_socket, (struct sockaddr *) &sin, 
			   &sin_size))>=0)
    {
      if (p_trace)
	*p_trace << "connection " << csocket << "\n";

      BufferedSocket bs(csocket);

      if (!check_cookie(bs))
	{
	  close(csocket);
	  continue;
	}
      
      handle_client(bs, handler);
      close(csocket);

      if (p_trace)
	*p_trace << "Client " << csocket << " disconnected\n";
    }

  EST_sys_error("error accepting connections");
}

bool EST_Server::check_cookie(BufferedSocket &socket)

{
  if (p_entry.cookie == "" || p_entry.cookie == "none")
    return TRUE;

  int eof;
  EST_String cookie = read_data(socket, cookie_term, eof);

  if (eof || cookie.at(0, 2) != "//" || cookie.after(0,2) != p_entry.cookie)
    {
      EST_warning("Bad cookie '%s'", (const char *)cookie.after(0,2));
      return FALSE;
    }

  return TRUE;
}

bool EST_Server::process_command(BufferedSocket &socket, EST_String command, RequestHandler &handler)
{
  handler.requestString=command;

  if (!parse_command(command,
		     handler.package,
		     handler.operation,
		     handler.args))
    {
      EST_warning("Bad Command '%s'", (const char *)command);
      return FALSE;
    }
  
  EST_String error_message = handler.process();
  
  if (error_message != "")
    return_error(socket, error_message);
  else
    return_value(socket, handler.res, TRUE);

  return TRUE;
}

void EST_Server::handle_client(BufferedSocket &socket, RequestHandler &handler)
{
  while (1==1)
    {
      int eof;
      EST_String command=read_data(socket, command_term, eof);

      if (eof)
	break;

      if (p_trace)
	*p_trace << "Got command " << command << "\n";

      if (!process_command(socket, command, handler))
	break;
    }
}

void EST_Server::return_error(BufferedSocket &socket, EST_String err)
{
  
  if (p_trace)
    *p_trace << "error in processing: " << err << "\n";
  write(socket, err, result_term);
  write(socket, "ERROR", status_term);
}

void EST_Server::return_value(BufferedSocket &socket, Result &res, bool final)
{
  EST_String resultString = build_result(res);
  
  if (p_trace)
    *p_trace << "Result: " << resultString << "\n";
  
  write(socket, resultString, result_term);
  write(socket, final?"OK":"VAL", status_term); 
}
