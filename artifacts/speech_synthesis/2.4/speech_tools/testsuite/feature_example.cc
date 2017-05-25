/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1996                            */
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

#include "EST_unix.h"
#include "EST_ling_class.h"


/** @name Feature and Val Classes Example Code
  */
//@{

int main(void)
{
  
  /** @name Adding basic information to an EST_Item
  * 
  * An item such as 
  * <graphic fileref="../arch_doc/eq01.gif" format="gif"></graphic> 
  * is constructed as follows: (note that
  * the attributes are in capitals by linguistic convention only:
  * attribute names are case sensitive and can be upper or lower
  * case).
  */
  //@{

  //@{ code
  EST_Item p;
  
  p.set("POS", "Noun");
  p.set("NAME", "example");
  p.set("FOCUS", "+");
  p.set("DURATION", 2.76);
  p.set("STRESS", 2);

  //@} code

  /** The type of the values in features is a
  * <classname>EST_Val</classname> class, which is a union which can
  * store ints, floats, EST_Strings, void pointers, and
  * <classname>EST_Features</classname>. The overloaded function
  * facility of C++ means that the <function>set()</function> can be
  * used for all of these. 
  */

  //@}

  /** @name Accessing basic information in an Item
    * 
    * When accessing the features, the type must be
    * specified. This is done most easily by using of a series of
    * functions whose type is coded by a capital letter:
    * </para>
    * <formalpara><title><function>F()</function></title><para> return value as a 
    * float</para></formalpara>
    * <formalpara><title><function>I()</function></title><para> return value as a
    *	    integer</para></formalpara>
    * <formalpara><title><function>S()</function></title><para> return value as a
    * <formalpara><title><function>A()</function></title><para> return value as a
    *       EST_Features</para></formalpara>
    * <para>
    */

  //@{

  //@{ code
  cout << "Part of speech for p is " << p.S("POS") << endl;
  cout << "Duration for p is " << p.F("DURATION") << endl;
  cout << "Stress value for p is " << p.I("STRESS") << endl;
  //@} code

  /** </para>
    * <SIDEBAR>
    * <TITLE>Output</TITLE>
    * <screen>
    * "Noun"
    * 2.75
    * 1
    * </screen>
    * </SIDEBAR>
    * <para>
    * A optional default value can be given if a result is always desired
    */

  //@{ code
  cout << "Part of speech for p is " 
      << p.S("POS") << endl;
  cout << "Syntactic Category for p is " 
      << p.S("CAT", "Noun") << endl; // noerror
  //@} code

  //@}

  /** @name Nested feature structures in items
    * 
    * Nested feature structures such as <xref linkend="eq11"> 
    * <example ID="eq11">
    *   <title>Example eq11</title>
    * <graphic fileref="../arch_doc/eq05.gif" format="gif"></graphic>
    * </example>
    * can be created in a number of ways:
    */
  //@{

  //@{ code
  
  p.set("NAME", "d");
  p.set("VOICE", "+");
  p.set("CONTINUANT", "-");
  p.set("SONORANT", "-");

  EST_Features f;  
  p.set("PLACE OF ARTICULATION", f); // copy in empty feature set here
  
  p.A("PLACE OF ARTICULATION").set("CORONAL", "+");
  p.A("PLACE OF ARTICULATION").set("ANTERIOR", "+");
  //@} code

  /** or by filling the values in an EST_Features object and
    * copying it in:
    */

  //@{ code
  EST_Features f2;
  
  f2.set("CORONAL", "+");
  f2.set("ANTERIOR", "+");
  
  p.set("PLACE OF ARTICULATION", f2);
  //@} code
	

  /** Nested features can be accessed by multiple calls to the
    * accessing commands:
    */
  
  //@{ code
  cout << "Anterior value is: " << p.A("PLACE OF ARTICULATION").S("ANTERIOR");
  cout << "Coronal value is: " << p.A("PLACE OF ARTICULATION").S("CORONAL");
  //@} code

  /** The first command is <function>A()</function> because PLACE is a
    * feature structure, and the second command is
    * <function>S()</function> because it returns a string (the
    * value or ANTRIOR or CORONAL). A shorthand is provided to
    * extract the value in a single statement:
    */

  //@{ code
  cout << "Anterior value is: " << p.S("PLACE OF ARTICULATION.ANTERIOR");
  cout << "Coronal value is: " << p.S("PLACE OF ARTICULATION.CORONAL");
  //@} code
	  
  /** Again, as the last value to be returned is a string
    * <function>S()</function> must be used. This shorthand can also be used
    * to set the features:
    */

  //@{ code
  
  p.set("PLACE OF ARTICULATION.CORONAL", "+");
  p.set("PLACE OF ARTICULATION.ANTERIOR", "+");
  //@} code

  /** this is the easiest and most commonly used method. */


  //@}

  /** @name Utility functions for items
    * 
    * The presence of a attribute can be checked using
    * <function>f_present()</function>, which returns true if the
    *  attribute is in the item:
    */
  //@{

  //@{ code
  cout << "This is true: " << p.f_present("PLACE OF ARTICULATION");
  cout << "This is false: " << p.f_present("MANNER");
  //@} code

  /** A attribute can be removed by <function>f_remove</function>
    */

  //@{ code
  p.f_remove("PLACE OF ARTICULATION");
  //@} code

  //@}
	  
  exit(0);

}
//@}
