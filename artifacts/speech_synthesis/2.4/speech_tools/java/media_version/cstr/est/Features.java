

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
 //  Pretended EST_Features class.                                         \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.est;

import java.lang.*;
import java.util.*;

import cstr.util.*;

public class Features
  implements Featured
{
  String [] cachedFeatureNames;

  public Features()
    {
      this(0L, true);
    }

  public Features(long handle)
    {
      this(handle, false);
    }

  Features(long handle, boolean m)
    {
    }

  protected void finalize() throws Throwable
    {
      super.finalize();
    }

  public String [] names()
    {
      if (cachedFeatureNames ==null)
	cachedFeatureNames = new String [0];
      return cachedFeatureNames;
    }

  public void getPaths(String prefix, Vector names, boolean paths, boolean leaves)
    {
    }

  public boolean present(String n)
    {
      return false;
    }

  public String getS(String n)
    {
      return "";
    }

  public String getS(String n, String def)
    {
      return def;
    }

  public String getFeature(String n)
    {
      return "";
    }

  public float getF(String n)
    {
      return (float)0.0;
    }

  public float getF(String n, float def)
    {
      return def;
    }

  public Object get(String n)
    {
      return null;
    }

  public Object get(String n, Object def)
    {
      return def;
    }

  public void set(String n, float val)
    {
    }

  public void set(String n, String val)
    {
    }

  static {
  }

}
