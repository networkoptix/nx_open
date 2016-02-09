
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\
 //                                                                        \\
 //                 Centre for Speech Technology Research                  \\
 //                      University of Edinburgh, UK                       \\
 //                        Copyright (c) 1996,1997                         \\
 //                         All Rights Reserved.                           \\
 //   Permission is hereby granted, free of charge, to use and distribute  \\
 //   this software and its documentation without restriction, including   \\
 //   without limitation the rights to use, copy, modify, merge, publish,  \\
 //   distribute, sublicense, and/or sell copies of this work, and to      \\
 //   permit persons to whom this work is furnished to do so, subject to   \\
 //   the following conditions:                                            \\
 //    1. The code must retain the above copyright notice, this list of    \\
 //       conditions and the following disclaimer.                         \\
 //    2. Any modifications must be clearly marked as such.                \\
 //    3. Original authors' names are not deleted.                         \\
 //    4. The authors' names are not used to endorse or promote products   \\
 //       derived from this software without specific prior written        \\
 //       permission.                                                      \\
 //   THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        \\
 //   DISCLAIM ALL WARRANTIES With REGARD TO THIS SOFTWARE, INCLUDING      \\
 //   ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   \\
 //   SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     \\
 //   FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    \\
 //   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   \\
 //   AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          \\
 //   ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       \\
 //   THIS SOFTWARE.                                                       \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\
 //                                                                        \\
 //                  Author: Richard Caley (rjc@cstr.ed.ac.uk)             \\
 //  --------------------------------------------------------------------  \\
 //  Mechanics of an enumerated type. Wrapped in a class for inheritance   \\
 //  purposes. NOte this is a public class all of whose mechanics are      \\
 //  package local, we can get hold of these from anywhere, but can't      \\
 //  do a thing with them, in fact only cstr.util.Enum is supposed to go   \\
 //  near this.                                                            \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.util ;

import java.lang.*;
import java.util.*;

public class EnumValues 
{
  protected String name;
  protected Hashtable table;
  protected int n;
  protected Enum [] values=null;

  EnumValues(String n)
    {
      name=n;
      table=new Hashtable();
    }

  int add(Enum v)
    {
      table.put(v.toString(),
		v);
      table.put(v.toString(true),
		v);
      if (v.info != Enum.class)
	return n++;
      else
	return -1;
    }

  void add(Enum v, String a)
    {
      table.put(a,v);
    }

  Enum getValue(String s)
    throws IllegalArgumentException
    {
      Enum v = (Enum)table.get(s);
      if (v==null)
	throw new IllegalArgumentException(name+":"+s);
      return v;
    }

  Enum [] getValues()
    {
      if (values == null)
	{
	  values = new Enum[n];
	  Enumeration vs = table.elements();
	  while (vs.hasMoreElements())
	    {
	      Enum e = (Enum)vs.nextElement();
	      if (e.index >=0)
		values[e.index] = e;
	    }
	}
      return values;
    }
}
