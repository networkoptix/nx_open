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
/* Lets see if we can break the TokenStream class                       */
/*                                                                      */
/************************************************************************/

#include <cstdlib>
#include "EST_Token.h"

static void binary_read_test();

static void find_tokens(EST_TokenStream &ts)
{
    // Count and display the tokens in this stream
    int tokens;

    for (tokens=0; !ts.eof(); tokens++)
	cout << ts.get().string() << endl;
    cout << "Total: " << tokens << endl << endl;;

}

int main(int argc,char **argv)
{
    // Simple program to read all the tokens in the named file
    // a print a summary of them
    (void)argc;
    (void)argv;
    EST_TokenStream ts;
    EST_String s;

    // Basic tokenizing tasks changing punctuation, whitespace and
    // single character symbols etc.
    s = "This is a test.";
    cout << "Test 1: " << quote_string(s) << endl;
    ts.open_string(s);
    find_tokens(ts);
    ts.close();

    s = "This (is) a test.";
    cout << "Test 2: " << quote_string(s) << endl;
    ts.open_string(s);
    find_tokens(ts);
    ts.close();

    s = "This (is) a test.";
    cout << "Test 3: " << quote_string(s) << endl;
    ts.open_string("This (is) a test.");
    ts.set_PrePunctuationSymbols("({[");
    ts.set_PunctuationSymbols(EST_Token_Default_PunctuationSymbols);
    find_tokens(ts);
    ts.close();

    s = "This (is) a test.";
    cout << "Test 4: " << quote_string(s) << endl;
    ts.open_string(s);
    ts.set_SingleCharSymbols("()");
    ts.set_PunctuationSymbols(EST_Token_Default_PunctuationSymbols);
    find_tokens(ts);
    ts.close();

    s = "This \"is a\" te\\\"st.";
    cout << "Test 5: " << quote_string(s) << endl;
    ts.open_string(s);
    ts.set_PrePunctuationSymbols(EST_Token_Default_PrePunctuationSymbols);
    ts.set_PunctuationSymbols(EST_Token_Default_PunctuationSymbols);
    find_tokens(ts);
    ts.close();

    s = "This \"is a\" te\\\"st.";
    cout << "Test 6: " << quote_string(s) << endl;
    ts.open_string(s);
    ts.set_quotes('"','\\');
    find_tokens(ts);
    ts.close();

    s = "This \"is \n\
a\" te\\\"st.";
    cout << "Test 7: " << quote_string(s) << endl;
    ts.open_string(s);
    ts.set_quotes('"','\\');
    find_tokens(ts);
    ts.close();

    // test of reading binary data
    binary_read_test();

    return 0;
}

EST_String make_tokbins(const EST_String& filename)
{
    FILE *fd;
    char buff[64];
    int a[2];
    int numbytes;
    // Make a buffer with both tokens and binary data
    sprintf(buff,"a buffer BINARY ");
    a[0] = 7;
    a[1] = -34;
    memmove(buff+16,a,sizeof(int)*2);
    sprintf(buff+16+(sizeof(int)*2)," and tokens");

    if ((fd=fopen(filename,"w")) == NULL)
    {
	cerr << "Token_regression: failed to open " << filename << endl;
	exit(-1);
    }
    
    numbytes = fwrite(buff,1,16+(sizeof(int)*2)+11,fd);
    fclose(fd);

    // Special constructions as the string contains nulls
    return EST_String(buff,numbytes,0,numbytes);
}

static void binary_read_test()
{
    // You can use fread to read directly from a token stream
    // but care should be take at the boundaries.  Reading a
    // token will always read the character following it.  By
    // convention it is recommended you include the single token
    // BINARY follow by a single space in the stream before each
    // binary section.
    int b[2];
    EST_String tokbinbuf;
    EST_TokenStream ts;

    tokbinbuf = make_tokbins("tmp/tokbin.dat");

    // Do the reading 

    cout << "Reading tokens and binary from string\n";
    
    ts.open_string(tokbinbuf);
    
    cout << ts.get() << endl;
    cout << ts.get() << endl;
    if (ts.get() != "BINARY")
    {
	cout << "failed to read binary data, missing BINARY token." << endl;
	exit(-1);
    }
    ts.fread(b,sizeof(int),2);
    cout << b[0] << endl;
    cout << b[1] << endl;
    cout << ts.get() << endl;
    cout << ts.get() << endl;
    ts.close();

    cout << "Reading tokens and binary from file\n";
    
    ts.open("tmp/tokbin.dat");
    
    cout << ts.get() << endl;
    cout << ts.get() << endl;
    if (ts.get() != "BINARY")
    {
	cout << "failed to read binary data, missing BINARY token." << endl;
	exit(-1);
    }
    ts.fread(b,sizeof(int),2);
    cout << b[0] << endl;
    cout << b[1] << endl;
    cout << ts.get() << endl;
    cout << ts.get() << endl;
    ts.close();

}


