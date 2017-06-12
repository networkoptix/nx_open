 /*************************************************************************/
 /*                                                                       */
 /*                Centre for Speech Technology Research                  */
 /*                     University of Edinburgh, UK                       */
 /*                      Copyright (c) 1995,1996                          */
 /*                        All Rights Reserved.                           */
 /*                                                                       */
 /*  Permission is hereby granted, free of charge, to use and distribute  */
 /*  this software and its documentation without restriction, including   */
 /*  without limitation the rights to use, copy, modify, merge, publish,  */
 /*  distribute, sublicense, and/or sell copies of this work, and to      */
 /*  permit persons to whom this work is furnished to do so, subject to   */
 /*  the following conditions:                                            */
 /*   1. The code must retain the above copyright notice, this list of    */
 /*      conditions and the following disclaimer.                         */
 /*   2. Any modifications must be clearly marked as such.                */
 /*   3. Original authors' names are not deleted.                         */
 /*   4. The authors' names are not used to endorse or promote products   */
 /*      derived from this software without specific prior written        */
 /*      permission.                                                      */
 /*                                                                       */
 /*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
 /*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
 /*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
 /*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
 /*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
 /*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
 /*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
 /*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
 /*  THIS SOFTWARE.                                                       */
 /*                                                                       */
 /*************************************************************************/
 /*                                                                       */
 /*                  Authors:  Richard Caley                              */
 /* -------------------------------------------------------------------   */
 /*  Label conversion main file                                           */
 /*                                                                       */
 /*************************************************************************/


#include <cstdlib>
#include "EST_error.h"
#include "EST_ling_class.h"
#include "EST_cmd_line.h"

int main(int argc, char *argv[])
{
    EST_String out_file, ext;
    EST_StrList files;
    EST_Option al;

    parse_command_line(argc, argv, 
       EST_String("Usage:   "
       "ch_utt  <input file> -o <output file> <options>\n"
       "Summary: change/copy utterance file\n"
       "use \"-\" to make input and output files stdin/out\n"
       "-h               Options help\n"
       "-f <string>	 Feature to use as item ID when merging utterances.\n"
       "-o <ofile>       output file name\n"
       "-otype <string>  output file type: \n"
       "-sysdir <string> Look for unqualified system entities in this directory"
       ) + options_utterance_filetypes_long(),
			files, al);

    EST_Utterance utt;
    EST_read_status rstat;

    EST_String feat = al.present("-f")?al.sval("-f"):EST_String("name");

    if (al.present("-sysdir"))
      utterance_xml_register_id("^\\([^/]*\\)",
			 al.sval("-sysdir") + "/\\1");

    rstat=utt.load(files.first());

    if (rstat == read_format_error)
      EST_error("Bad format in %s", (const char *)files.first());
    else if (rstat != read_ok)
      EST_sys_error("Error reading %s", (const char *)files.first());

    EST_Utterance u;

    EST_Litem *fp = files.head()->next();
    for(; fp != NULL; fp=fp->next())
      {
	rstat = u.load(files(fp));

	if (rstat == read_format_error)
	  EST_error("Bad format in %s", (const char *)files(fp));
	else if (rstat != read_ok)
	  EST_sys_error("Error reading %s", (const char *)files(fp));
    
	utterance_merge(utt, u, feat);
      }

    EST_String otype = al.present("-otype")? al.sval("-otype") : (EST_String)"est";

    if (al.present("-o"))
      utt.save(al.sval("-o"), otype);
    else
    {
      utt.save("-", otype);
    }
    
    return 0;
}

