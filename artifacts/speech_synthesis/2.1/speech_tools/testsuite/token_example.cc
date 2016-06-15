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
/*                 Author: Alan W Black                                 */
/*                   Date: May 1997                                     */
/************************************************************************/
/*                                                                      */
/* Example of reading a file using the tokenizer                        */
/*                                                                      */
/************************************************************************/

#include <cstdlib>
#include "EST_Token.h"

#if defined(DATAC)
#    define __STRINGIZE(X) #X
#    define DATA __STRINGIZE(DATAC)
#endif

int main(int argc,char **argv)
{
    // Simple program to read all the tokens in the named file
    // a print a summary of them
    EST_TokenStream ts;
    int tokens, alices, quotes;
    EST_Token t;
    EST_String fname;
    
    if (argc > 2)
    {
	cerr << argv[0] << ": wrong number of arguments\n";
	exit(-1);
    }
    else if (argc == 2)
	fname = argv[1];
    else
	fname = DATA "/alice";

    if (ts.open(fname) == -1)
    {
	cerr << argv[0] << ": can't open input file \"" << argv[1] <<
	    "\"\n";
	exit(-1);
    }

    // Control of whitespace characters, single character symbols,
    // pre and post punctuation may be set here.
    
    // The defaults are standard whitespace, and nothing for the rest
    // (this is like awk's basic tokenizer).  For language analysis
    // you'll probably want to modify the punctuation
    // \173 is '{', it is inserted by number because of a doc++ problem.

    ts.set_PrePunctuationSymbols("\173[(\"'");
    ts.set_PunctuationSymbols(EST_Token_Default_PunctuationSymbols);

    // Note you may set quotes so quoted tokens are read as single
    // tokens (a la C)

    for (tokens=quotes=alices=0; !ts.eof(); tokens++)
    {
	t = ts.get();
	if (t == "Alice")
	    alices++;
	if (t.prepunctuation().contains("\""))
	    quotes++;
    }

    printf("Input file contains:\n");
    printf("  %5d  tokens\n",tokens);
    printf("  %5d  tokens preceeded by double quotes\n",quotes);
    printf("  %5d  occurrences of Alice\n",alices);

    return 0;
}


