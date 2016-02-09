
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
 //                    Date: Tue Feb 17 1998                               \\
 //  --------------------------------------------------------------------  \\
 //  An array with only a few entries.                                     \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.util ;

import java.lang.*;
import java.util.*;
import java.awt.*;

public class SparseArray 
{
  int [] keys;
  Object [] vals;
  int used;
  int size;

  public SparseArray(int initn)
    {
      used=0;
      size = initn;
      keys = new int[size];
      vals = new Object[size];
    }

  public SparseArray()
    {
      this(10);
    }

  public void put(int n, Object v)
    {
      for(int i=0; i<used; i++)
	if (keys[i] == n)
	  {
	    vals[i] = v;
	    return;
	  }

      if (used == size)
	{
	  size+=10;
	  int [] newk = new int[size];
	  Object [] newv = new Object[size];

	  for(int j=0; j<used; j++)
	    {
	      newk[j] = keys[j];
	      newv[j] = vals[j];
	    }

	  keys=newk;
	  vals=newv;
	}
      keys[used] = n;
      vals[used] = v;
      used++;
    }

  public Object get(int n)
    {
      for(int i=0; i<used; i++)
	if (keys[i] == n)
	    return vals[i];
      return null;
    }
}
