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
 /* Table of known services.                                              */
 /*                                                                       */
 /*************************************************************************/
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <ctime>

using namespace std;

#include "EST_Pathname.h"
#include "EST_socket.h"
#include "EST_error.h"
#include "EST_Token.h"
#include "EST_ServiceTable.h"

#if defined(SYSTEM_IS_WIN32)
#    define seed_random() srand((unsigned)time(NULL))
#    define get_random() rand()
#else
#    define seed_random() srandom(time(NULL));
#    define get_random() random()
#endif

#if defined(__sun__) && defined(__sparc__) && defined(__svr4__)
/* Solaris */
extern "C" {
long random();
#if defined(older_solaris)
int srandom( unsigned seed);
int gethostname(char *name, int namelen);
#endif
}
#endif


EST_ServiceTable::EntryTable EST_ServiceTable::entries;
bool EST_ServiceTable::random_init=FALSE;

void EST_ServiceTable::init_random(void)
{
  if (!random_init)
    {
      random_init=TRUE;
      seed_random();
    }
}

bool operator == (const EST_ServiceTable::Entry &a, const EST_ServiceTable::Entry &b)
{
  return &a == &b;
}

EST_ServiceTable::Entry::Entry()
{
  name="";
  type="";
  hostname="";
  address="";
  cookie="";
  port=0;
}

EST_ServiceTable::Entry::operator EST_String() const
{
  return EST_String::cat("{ ServiceTable entry ",
			 type,
			 ":",
			 name,
			 " }");
}
    


void EST_ServiceTable::read_table(EST_String socketsFileName)
{
  EST_TokenStream str;
  FILE *sfile;

  if ((sfile = fopen(socketsFileName, "r"))==NULL)
    return;

  if (str.open(sfile, 1))
    EST_sys_error("Can't access fringe file '%s'", 
		  (const char *)socketsFileName);

  str.set_SingleCharSymbols("(){}[]#=.");

  while(!str.eof())
    {
      EST_Token tok = str.get();

      if (tok == "#")
	{
	  str.get_upto_eoln();
	  continue;
	}

      EST_String name = tok;

      str.must_get(".");

      tok = str.get();
      EST_String type = tok;

      str.must_get("=");

      EST_Token val = str.get_upto_eoln();

      if (!entries.t.present(name))
	{
	  Entry newent;
	  newent.name=name;
	  entries.t.add_item(name, newent);
	}

      Entry &entry = entries.t.val(name);

      if (type=="host")
	entry.hostname=(EST_String)val;
      else if (type=="address")
	entry.address=(EST_String)val;
      else if (type=="type")
	entry.type=(EST_String)val;
      else if (type=="port")
	entry.port=val;
      else if (type=="cookie")
	entry.cookie=(EST_String)val;
      else
	EST_warning("Unknown entry field '%s' at %s",
		    (const char *)type,
		    (const char *)str.pos_description());
    }

  str.close();
}

void EST_ServiceTable::write_table(EST_String filename)
{
  FILE *f;

  if ((f=fopen(filename, "w"))==NULL)
    EST_sys_error("can't write serice file");

  fprintf(f, "#Services\n");

  EntryTable::Entries p;

  for(p.begin(entries.t); p ; ++p)
    {
      const EST_String &name = p->k;
      const Entry &entry = p->v;

      fprintf(f, "%s.type=%s\n", (const char *)name, (const char *)entry.type);
      fprintf(f, "%s.cookie=%s\n", (const char *)name, (const char *)entry.cookie);
      fprintf(f, "%s.host=%s\n", (const char *)name, (const char *)entry.hostname);
      fprintf(f, "%s.address=%s\n", (const char *)name, (const char *)entry.address);
      fprintf(f, "%s.port=%d\n", (const char *)name, entry.port);
    }

  fclose(f);
}


void EST_ServiceTable::read_table()
{
  EST_Pathname socketsFileName(getenv("HOME")?getenv("HOME"):"/");

  socketsFileName += ".estServices";

  read_table(socketsFileName);
}

void EST_ServiceTable::write_table()
{
  EST_Pathname socketsFileName(getenv("HOME")?getenv("HOME"):"/");

  socketsFileName += ".estServices";

  write_table(socketsFileName);
}

void EST_ServiceTable::list(ostream &s, const EST_String type)
{

  EntryTable::KeyEntries keys(entries.t);
  char buff[31];

  s << "Known names:";
  while(keys.has_more_elements())
    {
      EST_String key = keys.next_element();
      s << " " << key;
    }

  s << "\n\n";

  if (type != "")
    s << "Entries of type " << type << ":\n";

  EntryTable::Entries them;

  //fmtflags old=s.flags();
  s.setf(ios::left);
  // (problems with iomanip in gcc-2.95.1 require oldfashioned solutions)

  s << "   ";
  sprintf(buff,"%10s","Name");
  s << buff;
  sprintf(buff,"%30s","    Hostname");
  s << buff;
  sprintf(buff,"%20s"," IP Address");
  s << buff;
  sprintf(buff,"%5s\n","Port");
  s << buff;

  for(them.begin(entries.t); them; them++)
    {
      const Entry &entry = them->v;

      if (type != "" && entry.type != type)
	continue;

      s << "   ";
      sprintf(buff,"%10s",(const char *)entry.name);
      s << buff;
      sprintf(buff,"%30s",(const char *)entry.hostname);
      s << buff;
      sprintf(buff,"%20s",(const char *)entry.address);
      s << buff;
      s << entry.port << "\n";
    }

  // s.flags(old);
}

void EST_ServiceTable::names(EST_TList<EST_String> &names, const EST_String type)
{
  names.clear();
  EntryTable::Entries them;
  for(them.begin(entries.t); them; them++)
    if (type=="" || type == them->v.name)
      names.append(them->k);
}

const EST_ServiceTable::Entry &EST_ServiceTable::lookup(const EST_String name,
						       const EST_String type)
{
  if (entries.t.present(name))
    {
      Entry &entry = entries.t.val(name);

      if (entry.type == type)
	return entry;
    }

  return *(entries.t.default_val);
}

const EST_ServiceTable::Entry &EST_ServiceTable::create(const EST_String name,
							const EST_String type,
							int socket)
{
  init_random();
  Entry entry;
  long cookie = get_random();
  struct sockaddr_in sin;

  socklen_t size=sizeof(struct sockaddr_in);

  // This only gets the port number

  if (getsockname(socket, (struct sockaddr *)&sin, &size) != 0)
    EST_sys_error("Can't find my address");


  // We have to get an address by looking ourselves up.
  char myname[100];

  gethostname(myname, 100);

  struct hostent *hent = gethostbyname(myname);

  if (hent == NULL)
    EST_sys_error("Can't look up my address");

  // Arbitrarily choose the first address.
  if (hent->h_addr_list != NULL)
    memcpy(&(sin.sin_addr.s_addr),hent->h_addr_list[0], sizeof (sin.sin_addr.s_addr));
  
  EST_String address= inet_ntoa(sin.sin_addr);

  entry.name = name;
  entry.type = type;
  entry.cookie = EST_String::Number(cookie);
  entry.port = ntohs(sin.sin_port);
  entry.address = address;
  entry.hostname = hent->h_name;

  entries.t.add_item(name, entry);

  return entries.t.val(name);
}



Declare_KVL_T(EST_String, EST_ServiceTable::Entry, EST_String_ST_entry)

#if defined(INSTANTIATE_TEMPLATES)

#include "../base_class/EST_TList.cc"
#include "../base_class/EST_TSortable.cc"
#include "../base_class/EST_TKVL.cc"

Instantiate_KVL_T(EST_String, EST_ServiceTable::Entry, EST_String_ST_entry)

#endif


