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
/*                 Author: Richard Caley                                */
/*                   Date: May 1997                                     */
/************************************************************************/
#include "EST_String.h"
#include <iostream>
#include <cstdio>

int main()
{
    EST_String example("hello world");
    EST_Regex exclamation("\\(wow\\|yey\\|lo\\)[^a-z]");

    cout << example << "\n";
    cout.flush();
    printf("stdio version %s\n", (const char *)example);
    fflush(stdout);

    if (example.contains(exclamation))
      cout << "\nYes, it contains a match for " << exclamation << "\n";

    // find a match and extract the thing in brackets
    int start_br[EST_Regex_max_subexpressions];
    int end_br[EST_Regex_max_subexpressions];
    int len;

    if (example.search(exclamation, len, 0, start_br, end_br)>=0)
      {
	// whole match is item 0
	cout << "match was '" << example.at(start_br[0], end_br[0]- start_br[0]) << "'\n";

	// first set of brackets.
	cout << " word was '" << example.at(start_br[1], end_br[1]- start_br[1]) << "'\n";
      }

    cout << "\n";

    // substituting into a string based on a regular expression
    EST_String source("http://www.cstr.ed.ac.uk/speech_tools");
    EST_Regex url_re("\\([a-z]*\\)://\\([^/]*\\)\\(.*\\)");
    EST_String target("protocol=\\1 host=\\2 path=\\3 dummy=\\6");

    cout << "processing '" <<source <<"'\n";
    if (source.matches(url_re,
		       0, 
		       start_br, end_br))
      {
	target.subst(source, start_br, end_br);
	cout << "   gives '" << target <<"'\n";
      }
    else
      cout <<"No match for URL RE\n";

    EST_String complex("what if I don't like 'hello world'?");

    EST_String quoted(complex.quote_if_needed('\''));
    EST_String unquoted(quoted.unquote_if_needed('\''));

    cout << "\n";
    cout << "start with \"" << complex << "\"\n";
    cout << "    quoted \"" << quoted << "\"\n";
    cout << "   unquoted \"" << unquoted << "\"\n";

#if 0
    EST_String gubbins = unquoted + quoted;

    cout << "   gubbins \"" << gubbins << "\"\n";
#endif

    return 0;
}
