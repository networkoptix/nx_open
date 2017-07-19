
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
 //                    Date: Wed Feb 25 1998                               \\
 //  --------------------------------------------------------------------  \\
 //  Items in a stream. Wrapper around EST_StreamItem.                     \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.est;

import java.lang.*;
import java.util.*;
import java.awt.*;

import cstr.util.*;

public class Item_Content
  implements Featured
{
  private long cpp_handle;
  private boolean mine;
  private String [] cachedFeatureNames;

  public Item_Content()
    {
      this(0L, true);
    }

  public Item_Content(long handle)
    {
      this(handle, false);
    }

  Item_Content(long handle, boolean m)
    {
      create_cpp_itemContent(handle);
      mine=m;
    }

  protected void finalize() throws Throwable
    {
      if (mine)
	destroy_cpp_itemContent();
      super.finalize();
    }

  private native long cpp_getItem();

  public Item getItem()
    {
      return new Item(cpp_getItem());
    }

  private native long cpp_getItem(String relName);

  public Item getItem(String relName)
    {
	long h = cpp_getItem(relName);
      return h==0L?null:new Item(h);
    }

  private native long cpp_getItem(long relh);

  public Item getItem(Relation rel)
    {
	long h = cpp_getItem(rel.getHandle());
	return h==0L?null:new Item(h);
    }

  private native String [] cpp_featureNames();

  public String [] featureNames()
    {
      if (cachedFeatureNames ==null)
	cachedFeatureNames = cpp_featureNames();
      return cachedFeatureNames;
    }

  private native boolean cpp_featurePresent(String n);

  public boolean featurePresent(String n)
    {
      return cpp_featurePresent(n);
    }

  private native String cpp_getS(String n, String def, long r);
  
  public String getS(String n, Relation r)
    {
      return cpp_getS(n, "", r.getHandle());
    }

  public String getSdef(String n, String def, Relation r)
    {
      return cpp_getS(n, def, r.getHandle());
    }

  public String getS(String n)
    {
      return cpp_getS(n, "", 0L);
    }

  public String getFeature(String n)
    {
      return cpp_getS(n, "", 0L);
    }

  private native float cpp_getF(String n, float def, long r);
  
  public float getF(String n, Relation r)
    {
      return cpp_getF(n,(float)0.0,r.getHandle());
    }

  public float getF(String n, float def, Relation r)
    {
      return cpp_getF(n,def,r.getHandle());
    }

  public float getF(String n)
    {
      return cpp_getF(n,(float)0.0,0L);
    }

  private native Object cpp_get(String n, Object def, long r);
  
  public Object get(String n, Relation r)
    {
      return cpp_get(n,null,r.getHandle());
    }

  public Object get(String n, Object def, Relation r)
    {
      return cpp_get(n,def,r.getHandle());
    }

  public Object get(String n)
    {
      return cpp_get(n,null,0L);
    }

  private native void cpp_set(String n, float val);
  
  public void set(String n, float val)
    {
      cpp_set(n, val);
    }

  private native void cpp_set(String n, String val);
  
  public void set(String n, String val)
    {
      cpp_set(n, val);
    }

  public String name()
    {
      return cpp_getS("name", "<NONAME>", 0L);
    }

  public String getName()
    {
      return cpp_getS("name", "<NONAME>", 0L);
    }

  private native long cpp_getFeatures();

  public Features getFeatures()
    {
      long fh = cpp_getFeatures();

      return new Features(fh);
    }

  private native float cpp_getStartTime();
  
  public float getStartTime()
    {
      return cpp_getStartTime();
    }

  private native float cpp_getMidTime();
  
  public float getMidTime()
    {
      return cpp_getMidTime();
    }

  private native float cpp_getTime();
  
  public float getTime()
    {
      return cpp_getTime();
    }

  private native float cpp_getEndTime();
  
  public float getEndTime()
    {
      return cpp_getEndTime();
    }

    public final int hashCode()
    {
      return (int)cpp_handle;
    }

  public boolean equals(Object i)
    {
      return i instanceof Item_Content && ((Item_Content)i).cpp_handle == cpp_handle;
    }

  private native static boolean initialise_cpp();
  private native static boolean finalise_cpp();
  private native boolean create_cpp_itemContent(long handle);
  private native boolean destroy_cpp_itemContent();

  static {
    System.loadLibrary("estjava");
    if (!initialise_cpp())
	throw new ExceptionInInitializerError("Item_Content C++ fails");
  }

}
