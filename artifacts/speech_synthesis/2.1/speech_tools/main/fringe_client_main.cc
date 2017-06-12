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
 /* Simple client which talks to fringe.                                  */
 /*                                                                       */
 /*************************************************************************/

#include <cstdlib>
#include "EST_unix.h"
#include "EST_error.h"
#include "EST_ServiceTable.h"
#include "EST_FringeServer.h"
#include "EST_cmd_line.h"

/** @name <command>fringe_client</command> <emphasis>Send commands to a running fringe server</emphasis>
    @id fringe-client-manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**

   <command>fringe_client</command> is a simple program for sending
   commands to a <command>fringe</command> process which is running in
   server mode.

 */

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main(int argc, char *argv[])
{
    EST_String out_file, ext;
    EST_StrList commands;
    EST_Option al;
    int verbose;

    parse_command_line
	(argc, argv, 
       EST_String("[options] [command]\n")+
       "Summary: Send commands to a running fringe server.\n"
       "use \"-\" to make input and output files stdin/out\n"
       "-h               Options help\n"
       "-n <string>	 Name of fringe to connect to (default 'fringe').\n"
       "-f <ifile>       File containing fringe connection information.\n"
       "-l               List available fringe servers.\n"
       "-v               Print what is being done.\n",
	 commands, al);

    verbose=al.present("-v");

    if (al.present("-f"))
      EST_ServiceTable::read_table(al.sval("-f"));
    else
      EST_ServiceTable::read_table();

    EST_String name="fringe";

    if (al.present("-n"))
      name = al.sval("-n");

    if (al.present("-l"))
      {
	EST_ServiceTable::list(cout, "fringe");
	exit(0);
      }

    EST_FringeServer server(name, verbose?&cout:(ostream *)NULL);

    switch (server.connect())
      { 
      case connect_ok:
	break;

      case connect_not_found_error:
	EST_sys_error("Can't find host '%s:%d'", (const char *)server.servername(), server.port());
	break;

      case connect_not_allowed_error:
	EST_sys_error("Can't connect to '%s:%d'", (const char *)server.servername(), server.port());
	break;

      default:
      EST_sys_error("Error connecting to  '%s:%d'", (const char *)server.servername(), server.port());
      break;
      }

    EST_StrList::Entries p;

    for(p.begin(commands); p != 0; ++p)
      {
	EST_String package;
	EST_String operation;
	EST_FringeServer::Args args;
	EST_FringeServer::ResultHandler res_hand;
	EST_FringeServer::Result &res = res_hand.res;

	if (server.parse_command(*p,
				 package,
				 operation,
				 args))
	  {
	    if (verbose)
	      {
		printf("command package='%s' operation='%s'\n",
		       (const char *)package,
		       (const char *)operation);

		EST_FringeServer::Args::Entries argp;
		
		for (argp.begin(args); argp != 0; ++argp)
		  printf("\t%10s%s%s\n", 
			 (const char *)argp->k,
			 argp->k==""?" : ":" = ",
			 (const char *)argp->v.String());
	      }

	    if (!server.execute(package,
				operation,
				args,
				res_hand))
	      EST_error("Error from Fringe: %s", 
			(const char *)res.S("ERROR"));
		
	  }
	else
	  EST_error("badly formatted command '%s'", (const char *)*p);
      }

    return(0);
}

/**@name Finding Fringe.

Each <command>fringe</command> which runs in server mode registers
it's location in a file called <filename>.estServices</filename> in
the users home directory. Multiple servers can be present if they are
given different names, and the <option>-n</option> can be used to
select which fringe a command is sent to.

*/
