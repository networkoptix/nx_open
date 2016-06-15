
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
 //                    Date: Tue Mar 31 1998                               \\
 //  --------------------------------------------------------------------  \\
 //  Java wrapper around items.                                        \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\

package cstr.est;

import java.lang.*;
import java.util.*;
import java.awt.*;

import cstr.util.*;
import java.io.*;

public class Item
                implements Named, Keyed, Featured
{
  public Item()
    {
      this(0L, true, null);
    }

  public Item(long handle)
    {
      this(handle, false, null);
    }

  public Item(long handle, Object utterance)
    {
      this(handle, false, utterance);
    }

  Item(long handle, boolean m, Object from_utterance)
    {
    }

  protected void finalize() throws Throwable
    {
      super.finalize();
    }

   protected final Item getItem(long handle)
    {
      return null;
    }

  public String name()
    {
      return null;
    }

  public String getName()
    {
      return null;
    }

  public void setName(String name)
    {
    }

  public final Item_Content getContent()
    {
      return null;
    }

  public final Object getKey()
    {
      return null;
    }

  public String getS(String n)
    {
      return null;
    }

  public String getS(String n, String def)
    {
      return def;
    }

  public String getFeature(String n)
    {
      return null;
    }

  public float getF(String n)
    {
      return (float)0.0;
    }

  public String type()
    {
      return null;
    }

  public float getStartTime()
    {
      return (float)0.0;
    }

  public float getMidTime()
    {
      return (float)0.0;
    }

  public float getEndTime()
    {
      return (float)0.0;
    }

  public Item next()
    {
      return null;
    }

  public Item prev()
    {
      return null;
    }

  public Item up()
    {
      return null;
    }

  public Item down()
    {
      return null;
    }
}
