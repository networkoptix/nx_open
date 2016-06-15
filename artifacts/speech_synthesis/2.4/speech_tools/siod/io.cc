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
 /************************************************************************/
 /*                 Author: Richard Caley (rjc@cstr.ed.ac.uk)            */
 /*                   Date: Tue Jun 10 1997                              */
 /************************************************************************/
 /*                                                                      */
 /* Functions to open file descriptors for various kinds of data         */
 /* sources and sinks.                                                   */
 /*                                                                      */
 /************************************************************************/

#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include "EST_unix.h"
#include "EST_socket.h"

#include <sys/types.h>

#include "EST_String.h"
#include "EST_bool.h"
#include "siod.h"
#include "siodp.h"
#include "io.h"

EST_Regex RxURL("\\([a-z]+\\)://?\\([^/:]+\\)\\(:\\([0-9]+\\)\\)?\\(.*\\)");
EST_Regex RxFILEURL("file:.*");
static EST_Regex ipnum("[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+");

const int default_http_port = 80;
//const int default_ftp_port = 21;

#define MAX_LINE_LENGTH (256)

static int port_to_int(const char *port)
{
  struct servent *serv;

  if (!port || *port == '\0')
    return -1;

  if ((serv=getservbyname(port,  "tcp")))
    return serv->s_port;

  return atoi(port);
}

int parse_url(const EST_String &url,
	       EST_String &protocol, 
	       EST_String &host, 
	       EST_String &port, 
	       EST_String &path)
{
  EST_String bitpath;
  int start_of_bracket[EST_Regex_max_subexpressions];
  int end_of_bracket[EST_Regex_max_subexpressions];

  if (url.matches(RxFILEURL,0,start_of_bracket, end_of_bracket))
  {
    protocol = "file";
    host = "";
    port = "";
    path = url.after("file:");
    return TRUE;
  }
  else  if (!url.matches(RxURL, 0, start_of_bracket, end_of_bracket))
    return FALSE;

  protocol = url.at(start_of_bracket[1], end_of_bracket[1]-start_of_bracket[1]);
  host = url.at(start_of_bracket[2], end_of_bracket[2]-start_of_bracket[2]);
  port = url.at(start_of_bracket[4], end_of_bracket[4]-start_of_bracket[4]);
  bitpath = url.at(start_of_bracket[5], end_of_bracket[5]-start_of_bracket[5]);

  if (protocol == "http")
      path = protocol + "://" + host + bitpath;
  else
      path = bitpath;

  return TRUE;
}

static int connect_to_server(const char *host, int port)
{
  struct sockaddr_in address; 
  struct hostent *hostentp;
  EST_String shost=host;
  int s;
  
  memset(&address, 0, sizeof(address));

  if (shost.matches(ipnum))
  {  
    address.sin_addr.s_addr = inet_addr(host);
    address.sin_family = AF_INET;
  }
  else if ((hostentp=gethostbyname(host))==NULL)
	err("can't find host", host);
  else
  {
    memset(&(address.sin_addr),0,sizeof(struct in_addr));
    address.sin_family=hostentp->h_addrtype;
    memmove(&address.sin_addr, 
	    (hostentp->h_addr_list)[0],
	    hostentp->h_length);
  }
  address.sin_port=htons(port);

  if ((s=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    err("can't create socket", NIL);

  if (connect(s, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
	  close(s);
	  err("can't connect to host", 
	      inet_ntoa(address.sin_addr));
	}

  return s;
}

static void server_send(int s, const char *text)
{
  size_t n=strlen(text);
  ssize_t sent;

  while (n>0)
    if ((sent = write(s, text, n))<0)
      err("error talking to server", NIL);
    else
      n -= sent;
}

static const char *server_get_line(int s)
{
  static char buffer[MAX_LINE_LENGTH+1];
  char *p=buffer;
  ssize_t n;

  *p='\0';

  while(1==1)
    if ((n=read(s, p, 1)) == 0)
      break;
    else if (n < 0)
      err("error while reading from server", NIL);
    else if (*(p++) == '\n')
      break;

  *p = '\0';
      
  return buffer;
}


/*
 * Open stdin or stdout. Should this do a dup?
 */

int fd_open_stdinout(const char *r_or_w)
{
  int fd = -1;

  if (r_or_w[0] == 'r')
    fd = fileno(stdin);
  else if (r_or_w[0] == 'w')
    fd = fileno(stdout);
  else
    err("mode not understood for -", r_or_w);
  return fd;
}

/*
 * Duplicates the fopen interpretation of the type
 * parameter plus "rw" being a synonym for "r+" to preserve
 * some scheme semantics.
 */
int fd_open_file(const char *name, const char *r_or_w)
{
  int fd;
  int mode=0;
  int go_to_end=0;

  if (strcmp(name, "-")==0)
    return fd_open_stdinout(r_or_w);

  if (r_or_w[0] == 'r')
    if (r_or_w[1] == '+' || r_or_w[1] == 'w')
      mode = O_RDWR|O_CREAT;
    else
      mode = O_RDONLY;
  else if (r_or_w[0] == 'w')
      if (r_or_w[1] == '+')
	mode = O_RDWR|O_CREAT|O_TRUNC;
      else
	mode = O_WRONLY|O_CREAT|O_TRUNC;
  else if (r_or_w[0] == 'a')
    if (r_or_w[1] == '+')
      go_to_end = mode = O_RDWR;
    else
      go_to_end = mode = O_WRONLY|O_CREAT;
  else
    err("mode not understood", r_or_w);

  /* Should deal with `b' here for binary files.
   */
  
  fd= open(name, mode, 0666);

  if (fd >=0 && go_to_end)
    lseek(fd, 0, SEEK_END);

  return fd;
}

int fd_open_http(const char *host,
		 int port,
		 const char *path,
		 const char *r_or_w)
{
  int s;

  if (port <0)
    port=default_http_port;

  if ((s=connect_to_server(host, port)) < 0)
    return s;

  if (*r_or_w == 'r')
    {
      const char *line;
      float http_version;
      int   code;
      char  location[1024] = "";

      server_send(s, "GET ");
      server_send(s, path);
      server_send(s, " HTTP/1.0\n\n");
      shutdown(s, 1);

      line= server_get_line(s);

      if (sscanf(line, "HTTP/%f %d", &http_version, &code) != 2)
	{
	  close(s);
	  err("HTTP error", line);
	}

      // Skip rest of header.
      while((line = server_get_line(s)))
	{
	  if (*line=='\r' || *line == '\n' || *line == '\0')
	    break;
	  else if (sscanf(line, "Location: %s", location) == 1)
	    {
	      cout << "redirect to '" << location << "'\n";
	    }
	}

      if (code == 301 || code == 302)
	{
	  close(s);
	  
	  if (*location == '\0')
	    err("Redirection to no loction", NIL);


	  EST_String sprotocol, shost, sport, spath;

	  if (!parse_url(location, sprotocol, shost, sport, spath))
	    err("redirection to bad URL", location);
	  
	  s = fd_open_url(sprotocol, shost, sport, spath, "rb");
	}
      
    }
  else if (*r_or_w == 'w')
    err("Write to HTTP url not yet implemented", NIL);

  return s;
}

int fd_open_ftp(const char *host,
		 int port,
		 const char *path,
		 const char *r_or_w)
{
  (void)host;
  (void)port;
  (void)path;
  (void)r_or_w;

  return -1;
}

int fd_open_tcp(const char *host,
		 int port,
		const char *text,
		 const char *r_or_w)
{
  int s;

  if (port <0)
    return -1;

  if ((s=connect_to_server(host, port)) < 0)
    return s;

  server_send(s, text);

  if (*r_or_w == 'r')
    shutdown(s, 1);
  else if (*r_or_w == 'w')
    shutdown(s, 0);

  return s;
}

/*
 * Open a stream to a URL.
 */

int fd_open_url(const char *protocol,
		const char *host,
		const char *port,
		const char *path,
		const char *r_or_w)
{
  // special case for local file URLs
  if (strcmp(protocol, "file") == 0 
      && (!host || *host == '\0')
      && (!port || *port == '\0'))
    return fd_open_file(path, r_or_w);
  else if (strcmp(protocol, "file") == 0 || strcmp(protocol, "ftp") == 0)
    return fd_open_ftp(host, port_to_int(port), path, r_or_w);
  else if (strcmp(protocol, "http") == 0)
    return fd_open_http(host, port_to_int(port), path, r_or_w);
  else if (strcmp(protocol, "tcp") == 0)
    return fd_open_tcp(host, port_to_int(port), path, r_or_w);
  else
    return -1;
}
