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


/** @name Linguistic Classes Example Code
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
	  
  /** @name Building a linear list relation
    *  <!--  *** UPDATE *** -->      
    * 	
    * 	It is standard to store the phones for an utterance as a linear list
    * 	in a EST_Relation object. Each phone is represented by one
    * 	EST_Item, whereas the complete list is stored as a
    * 	EST_Relation.
    * 	</para><para>
    * 	The easiest way to build a linear list is by using the
    * 	<function>EST_Relation.append()</function>, which when called
    * 	without arguments, makes a new empty EST_Item, adds it onto
    * 	the end of the relation and returns a pointer to it. The
    * 	information relevant to that phone can then be added to the
    * 	returned item.
    */
  //@{

  //@{ code
  EST_Relation phones;
  EST_Item *a;
  
  a = phones.append();
  
  a->set("NAME", "f");
  a->set("TYPE", "consonant");
  
  a = phones.append();
  
  a->set("NAME", "o");
  a->set("TYPE", "vowel");
  
  a = phones.append();
  
  a->set("NAME", "r");
  a->set("TYPE", "consonant");
  //@} code
  
  /** Note that the -> operator is used because the EST_Item a is a
    * pointer here. The same pointer variable can be used multiple
    * times because every time <function>append()</function> is
    * called it allocates a new item and returns a pointer to it.
    * </para><para>
    * If you already have a EST_Item pointer and want to add it to a
    * relation, you can give it as an argument to
    * <function>append()</function>, but this is generally
    * inadvisable as it involves some unnecessary copying, and also
    * you have to allocate the memory for the next EST_Item pointer
    * yourself every time (if you don't you will overwrite the
    * previous one):
    */
	
  //@{ code
  a = new EST_Item;
  a->set("NAME", "m");
  a->set("TYPE", "consonant");
  
  phones.append(a);
  
  a = new EST_Item;
  a->set("NAME", "ei");
  a->set("TYPE", "vowel");
  //@} code
	
  /** Items can be prepended in exactly the same way:
    */
  //@{ code

  a = phones.prepend();
  
  a->set("NAME", "n");
  a->set("TYPE", "consonant");
  
  a = phones.prepend();
  
  a->set("NAME", "i");
  a->set("TYPE", "vowel");
	
  //@} code
      
  //@}
    

  /** @name Iterating through a linear list relation
    * Iteration in lists is performed with
    * <function>next()</function> and <function>prev()</function>, and
    * an EST_Item, used as an iteration pointer.
    */
  //@{

  //@{ code
  EST_Item *s;

  for (s = phones.head(); s != 0; s = s->next())
    cout << s->S("NAME") << endl;
  //@} code

  /** </para>
    * <SIDEBAR>
    * <TITLE>Output</TITLE>
    * <screen>
    * name:i    type:vowel
    * name:n    type:consonant
    * name:f    type:consonant
    * name:o    type:vowel
    * name:r    type:consonant
    * name:m    type:consonant
    * </screen>
    * </SIDEBAR>
    * <para>
    */
  //@{ code

  for (s = phones.tail(); s != 0; s = s->prev())
    cout << s->S("NAME") << endl;

  //@} code

  /** </para>
    * <SIDEBAR>
    * <TITLE>Output</TITLE>
    * <screen>
    * name:m    type:consonant
    * name:r    type:consonant
    * name:o    type:vowel
    * name:f    type:consonant
    * name:n    type:consonant
    * name:i    type:vowel
    * </screen>
    * </SIDEBAR>
    *
    *<para> 	
    * <function>head()</function> and <function>tail()</function>
    * return EST_Item pointers to the start and end of the list.
    * <function>next()</function> and <function>prev()</function>
    * returns the next or previous item in the list, and returns
    * <literal>0</literal> when the end or start of the list is
    * reached. Hence checking for <literal>0</literal> is a useful
    * termination condition of the iteration. Taking advantage of C
    * shorthand allows us to write:
    */

  //@{ code
  for (s = phones.head(); s; s = s->next())
    cout << s->S("NAME") << endl;
  //@} code
  
  //@}

  /** @name Building a tree relation
    * 
    * <!--  *** UPDATE *** -->
    * 
    * 	It is standard to store information such as syntax as a tree
    * 	in a EST_Relation object. Each tree node is represented by one
    * 	EST_Item, whereas the complete tree is stored as a
    * 	EST_Relation.
    * </para><para>	
    * 	The easiest way to build a tree is by using the
    * 	<function>append_daughter()</function>, which when called
    * 	without arguments, makes a new empty EST_Item, adds it as a
    * 	daughter to an existing item and returns a pointer to it. The
    * 	information relevant to that node can then be added to the
    * 	returned item. The root node of the tree must be added
    * 	directly to the EST_Relation.
    */
  //@{
	
  //@{ code
  //@example prog01
  EST_Relation tree;
  EST_Item *r, *np, *vp, *n;
  
  r = tree.append();
  r->set("CAT", "S");
  
  np = append_daughter(r);
  np->set("CAT", "NP");
  
  n =  append_daughter(np);
  n->set("CAT", "PRO");
  
  n =  append_daughter(n);
  n->set("NAME", "John");
  
  vp = append_daughter(r);
  vp->set("CAT", "VP");
  
  n = append_daughter(vp);
  n->set("CAT", "VERB");
  n = append_daughter(n);
  n->set("NAME", "loves");
  
  np = append_daughter(vp);
  np->set("CAT", "NP");
  
  n = append_daughter(np);
  n->set("CAT", "DET");
  n = append_daughter(n);
  n->set("NAME", "the");
  
  n = append_daughter(np);
  n->set("CAT", "NOUN");
  n = append_daughter(n);
  n->set("NAME", "woman");
  
  cout << tree;
  //@} code

  /** </para>
    * <SIDEBAR>
    * <TITLE>Output</TITLE>
    * <screen>
    * (S 
    *   (NP 
    *      (N (John))
    *   )
    *   (VP 
    *      (V (loves)) 
    *      (NP 
    *         (DET the) 
    *         (NOUN woman))
    *   )
    *)
    *</screen>
    * </SIDEBAR>
    * <para>
    * Obviously, the use of recursive functions in building trees is more
    * efficient and would eliminate the need for the large number of
    * temporary variables used in the above example.
    */
  //@}

  /** @name Iterating through a tree relation
    * 
    * Iteration in trees is done with <function>daughter1()</function>
    * <function>daughter2()</function> <function>daughtern()</function> and
    * <function>parent()</function>. Pre-order traversal can be achieved
    * iteratively as follows:
    */
  //@{

  //@{ code
  n = tree.head();             // initialise iteration variable to head of tree 
  while (n)
    {
      if (daughter1(n) != 0) // if daughter exists, make n its daughter 
        n = daughter1(n);
      else if (n->next() != 0)//otherwise visit its sisters 
        n = n->next();
      else                    // if no sisters are left, go back up the tree 
	{                       // until a sister to a parent is found 
	  bool found=FALSE;
	  for (EST_Item *pp = parent(n); pp != 0; pp = parent(pp))
	    if (pp->next())
	      {
		n = pp->next();
		found=TRUE;
		break;
	      }
	  if (!found)
	    {
	      n = 0;
	      break;
	    }
	}
      cout << *n;
    }
  //@} code

  /** A special set of iterators are available for traversal of the leaf
    * (terminal) nodes of a tree:
    */

  //@{ code
  //@ example prog02
  //@ title Leaf iteration

  for (s = first_leaf(tree.head()); s != last_leaf(tree.head()); 
       s = next_leaf(s))
    cout << s->S("NAME") << endl;
  //@} code

  //@}

  /** @name Building a multi-linear relation
    */
  //@{

  //@}

/** @name Iterating through a multi-linear relation
  */
  //@{

  //@}

/** @name Relations in Utterances
  * 
  * The <classname>EST_Utterance</classname> class is used to store all
  * the items and relations relevant to a single utterance. (Here
  * utterance is used as a general linguistic entity - it doesn't have to
  * relate to a well formed complete linguistic unit such as a sentence or
  * phrase). 
  * </para><para>
  * Instead of storing relations separately, they are stored in
  * utterances:
  */
  //@{

  //@{ code
  EST_Utterance utt;
  
  utt.create_relation("Word");
  utt.create_relation("Syntax");
  //@} code

  /** EST_Relations can be accessed though the utterance object either
    * directly or by use of a temporary EST_Relation pointer:
    */

  //@{ code
  EST_Relation *word, *syntax;
  
  word = utt.relation("Word");
  syntax = utt.relation("Syntax");
  //@} code

  /** The contents of the relation can be filled by the methods described
    * above. 
    */
  
  //@}

  /** @name Adding items into multiple relations
    *
    * A major aspect of this system is that an item can be in two relations
    * at once, as shown in <xref linkend="figure02">.
    * </para><para>
    * In the following example, using the syntax relation as already created
    * in <xref linkend="prog01">,
    * shows how to put the terminal nodes of this
    * tree into a word relation:
    */
  //@{
  
  //@{ code
  //@example prog03
  //@title adding existing items to a new relation
  word = utt.relation("Word");
  syntax = utt.relation("Syntax");
  
  for (s = first_leaf(syntax->head()); s != last_leaf(syntax->head()); 
       s = next_leaf(s))
    word->append(s);
  
  //@} code

  /** 
    * Thus the terminal nodes in the syntax relation are now stored as a
    * linear list in the word relation.
    * 
    * Hence
    */

  //@{ code
  cout << *utt.relation("Syntax") << "\n";
  //@} code

  /** produces
    *</para>
    * <sidebar>
    * <title>Output</title>
    * <screen>
    *(S 
    *   (NP 
    *      (N (John))
    *   )
    *   (VP 
    *      (V (loves)) 
    *      (NP 
    *         (DET the) 
    *         (NOUN woman))
    *   )
    *)
    *</screen>
    *</sidebar>
    *<para>
    *whereas
    */

  //@{ code
  cout << *utt.relation("Word") << "\n";
  //@} code

  /** produces
    *</para>
    * <sidebar>
    * <title>Output</title>
    * <screen>
    *John
    *loves
    *the
    *woman
    *</screen>
    * </sidebar>
    * <para>
    */

  //@}


  /** @name Changing the relation an item is in
      as_relation, in relation etc
    */
  //@{

  //@}

  /** @name Feature functions
      evaluate functions
      setting functions
    */
  //@{
  

  //@}

  exit(0);

}
//@}
