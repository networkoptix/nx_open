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

int main()

{
EST_String line("\n");
EST_String zeroth("");
EST_String first("hello world");
EST_String second("lo w");
EST_String third("l");
EST_String fourth("the lazy dog.");
EST_String fifth("two\nlines");
EST_String sixth("-o:F");
EST_String seventh("-o");
EST_String eighth("some,words-with[punctuation]left..after,a-vowel!");
EST_String space(" ");
EST_String quoted("\"some tokens\" which  are \"quoted with \"\"\"");

EST_String bits1[10], bits2[10], bits3[10], bits4[10], bits5[10], bits6[2];

EST_String sub1 = first;
EST_String sub2 = first;
 
EST_Regex reg0(".*");
EST_Regex reg1("l+");
EST_Regex reg2("l\\(l+\\|azy\\)");
EST_Regex reg3("lll+");
EST_Regex reg4(second);
EST_Regex reg5(".*l+.*l+.*");
EST_Regex reg6(".. ..");
EST_Regex reg7("[a-z]\\.[a-z]");
EST_Regex reg8("o\\>");
EST_Regex reg9("ll\nrr");
EST_Regex reg10("o\nl");
EST_Regex reg11("\\([^aeiou]\\)\\(\\]\\|[-[.,!?]\\)+");

EST_String result0 = zeroth.before(".", -1);
EST_String result1 = first.before(second);
EST_String result2 = first.before(second,4);
EST_String result3 = first.before(third,4);
EST_String result4 = first.before(third, -6);
EST_String result5 = first.before(third, -7);
EST_String result6b = first.before(5);

EST_String result1a = first.after(second);
EST_String result2a = first.after(second,4);
EST_String result3a = first.after(third,4);
EST_String result4a = first.after(third, -6);
EST_String result5a = first.after(third, -7);
EST_String result6a = first.after(5);

EST_String result6 = second;
EST_String result7 = second;
result6 += " sw eet";
result7 += third;

int test0 = zeroth.contains(reg0);
int test1 = first.contains(reg1);
int test2 = first.contains(reg2);
int test3 = first.contains(reg3);
int test4 = first.contains(second);
int test5 = fourth.contains(reg2);
int test6 = fourth.contains(reg7);
int test7 = first.contains(reg8);
int test8 = fourth.contains(reg8);
int test9 = first.contains(reg9);
int test10 = fifth.contains(reg10);
int test11 = first.contains(second,3);
int test12 = first.contains(second,0);
int test13 = second.contains(third, 0);
int test14 = sixth.contains(seventh, 0);
int test15 = seventh.contains(seventh, 0);

int test0m = zeroth.matches(reg0);
int test1m = first.matches(reg4);
int test2m = second.matches(reg4);
int test3m = first.matches(reg5);

EST_String result1r = first.before(second);
EST_String result2r = first.before(third, -1);
EST_String result3r = first.after(third, 5);

EST_String result1at = first.at(second);
EST_String result2at = first.at(reg6);
EST_String result3at = first.at(2,4);

EST_String result8 = eighth;
result8.gsub(reg11,1);


int num1 = split(first, bits1, 10, reg1);
int num2 = split(first, bits2, 2, reg1);
int num7 = split(first, bits3, 10, space);
int num8 = split(quoted, bits4, 10, space, '"');
int num9 = split(quoted, bits5, 10, RXwhite, '"');
int num10 = split(first, bits6, 2, ".");

int num3 = first.freq("o");
int num4 = first.freq(third);
// numx = first.freq(reg1);	// GNU can't do this

int num5 = sub1.gsub("l", "[an ell]");
int num6 = sub2.gsub(reg1, "[some ells]");

cout << "First '"<< first << "'\n";
cout << "Second '"<< second << "'\n";
cout << "Third '"<< third << "'\n";

cout << "Result 0 '"<< result0 << "'\n";

cout << "Result 1 '"<< result1 << "'\n";
cout << "Result 2 '"<< result2 << "'\n";
cout << "Result 3 '"<< result3 << "'\n";
cout << "Result 4 '"<< result4 << "'\n";
cout << "Result 5 '"<< result5 << "'\n";
cout << "Result 6b '"<< result6b << "'\n";

cout << "Result 1a '"<< result1a << "'\n";
cout << "Result 2a '"<< result2a << "'\n";
cout << "Result 3a '"<< result3a << "'\n";
cout << "Result 4a '"<< result4a << "'\n";
cout << "Result 5a '"<< result5a << "'\n";
cout << "Result 6a '"<< result6a << "'\n";

cout << "Result 6 '"<< result6 << "'\n";
cout << "Result 7 '"<< result7 << "'\n";
cout << "Result 8 '"<< result8 << "'\n";

cout << "Test 0 '"<< test0 << "'\n";
cout << "Test 1 '"<< test1 << "'\n";
cout << "Test 2 '"<< test2 << "'\n";
cout << "Test 3 '"<< test3 << "'\n";
cout << "Test 4 '"<< test4 << "'\n";
cout << "Test 5 '"<< test5 << "'\n";
cout << "Test 6 '"<< test6 << "'\n";
cout << "Test 7 '"<< test7 << "'\n";
cout << "Test 8 '"<< test8 << "'\n";
cout << "Test 9 '"<< test9 << "'\n";
cout << "Test 10 '"<< test10 << "'\n";
cout << "Test 11 '"<< test11 << "'\n";
cout << "Test 12 '"<< test12 << "'\n";
cout << "Test 13 '"<< test13 << "'\n";
cout << "Test 14 '"<< test14 << "'\n";
cout << "Test 15 '"<< test15 << "'\n";

cout << "Test 0m '"<< test0m << "'\n";
cout << "Test 1m '"<< test1m << "'\n";
cout << "Test 2m '"<< test2m << "'\n";
cout << "Test 3m '"<< test3m << "'\n";

cout << "Result 1r '"<< result1r << "'\n";
cout << "Result 2r '"<< result2r << "'\n";
cout << "Result 3r '"<< result3r << "'\n";

cout << "Result 1at '"<< result1at << "'\n";
cout << "Result 2at '"<< result2at << "'\n";
cout << "Result 3at '"<< result3at << "'\n";

cout << "Num 1 '"<< num1 << "'\n";
cout << "bits1[0] '"<<bits1[0] << "'\n";
cout << "bits1[1] '"<<bits1[1] << "'\n";
cout << "bits1[2] '"<<bits1[2] << "'\n";

cout << "Num 2 '"<< num2 << "'\n";
cout << "bits2[0] '"<<bits2[0] << "'\n";
cout << "bits2[1] '"<<bits2[1] << "'\n";
cout << "bits2[2] '"<<bits2[2] << "'\n";

cout << "Num 7 '"<< num7 << "'\n";
cout << "bits3[0] '"<<bits3[0] << "'\n";
cout << "bits3[1] '"<<bits3[1] << "'\n";
cout << "bits3[2] '"<<bits3[2] << "'\n";

cout << "Num 8 '"<< num8 << "'\n";
cout << "bits4[0] '"<<bits4[0] << "'\n";
cout << "bits4[1] '"<<bits4[1] << "'\n";
cout << "bits4[2] '"<<bits4[2] << "'\n";
cout << "bits4[3] '"<<bits4[3] << "'\n";
cout << "bits4[4] '"<<bits4[4] << "'\n";
cout << "bits4[5] '"<<bits4[5] << "'\n";

cout << "Num 9 '"<< num9 << "'\n";
cout << "bits5[0] '"<<bits5[0] << "'\n";
cout << "bits5[1] '"<<bits5[1] << "'\n";
cout << "bits5[2] '"<<bits5[2] << "'\n";
cout << "bits5[3] '"<<bits5[3] << "'\n";
cout << "bits5[4] '"<<bits5[4] << "'\n";

cout << "Num 10 '"<< num10 << "'\n";
cout << "bits6[0] '"<<bits6[0] << "'\n";
cout << "bits6[1] '"<<bits6[1] << "'\n";

cout << "Num 3 '"<< num3 << "'\n";
cout << "Num 4 '"<< num4 << "'\n";

cout << "Num 5 '"<< num5 << "'\n";
cout << "Sub 1 '"<< sub1 << "'\n";

cout << "Num 6 '"<< num6 << "'\n";
cout << "Sub 1 '"<< sub2 << "'\n";

return (0);
}

