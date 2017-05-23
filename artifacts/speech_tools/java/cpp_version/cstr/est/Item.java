
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
  private long cpp_handle;
  private boolean mine;
  private Utterance utterance;
  private Item_Content contents;

  public Item()
    {
      this(0L, true, null);
    }

  public Item(long handle)
    {
      this(handle, false, null);
    }

  public Item(long handle, Utterance utterance)
    {
      this(handle, false, utterance);
    }

  Item(long handle, boolean m, Utterance from_utterance)
    {
      utterance = from_utterance;
      create_cpp_item(handle);
      mine=m;
    }

  protected void finalize() throws Throwable
    {
      if (mine)
	destroy_cpp_item();
      super.finalize();
    }

   protected final Item getItem(long handle)
    {
      Item i;
      if (utterance != null)
	i = utterance.getItem(handle);
      else
	  i = new Item(handle);
      return i;
    }

  public long getHandle()
    {
	return cpp_handle;
    }

  private native String cpp_name();

  public String name()
    {
      return cpp_name();
    }

  public String getName()
    {
      return cpp_name();
    }

  private native void cpp_setName(String name);
  
  public void setName(String name)
    {
      cpp_setName(name);
    }

  private native long cpp_getContent();

  public final Item_Content getContent()
    {
      if (contents==null)
	contents = new Item_Content(cpp_getContent());
      return contents;
    }

  public final Object getKey()
    {
      return getContent();
    }

  public final int hashCode()
    {
      return (int)cpp_getContent();
    }

  public final boolean equals(Object i)
    {
      return i instanceof Item && ((Item)i).cpp_handle == cpp_handle;
    }

  private native String cpp_getS(String n, String def);
  
  public String getS(String n, String def)
    {
      return cpp_getS(n, def);
    }

  public String getS(String n)
    {
      return cpp_getS(n, "");
    }

  public String getFeature(String n)
    {
      return cpp_getS(n, "");
    }

  private native float cpp_getF(String n, float def);
  
  public float getF(String n, float def)
    {
      return cpp_getF(n, def);
    }

  public float getF(String n)
    {
      return cpp_getF(n, 0);
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

  private native String cpp_type();
  
  public String type()
    {
      return cpp_type();
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

  private native long cpp_next();

  public Item next()
    {
      long h = cpp_next();
      if (h==0)
	return null;
      else
	return getItem(h);
    }

  private native long cpp_prev();

  public Item prev()
    {
      long h = cpp_prev();
      if (h==0)
	return null;
      else
	return getItem(h);
    }

  private native long cpp_up();

  public Item up()
    {
      long h = cpp_up();
      if (h==0)
	return null;
      else
	return getItem(h);
    }

  private native long cpp_down();

  public Item down()
    {
      long h = cpp_down();
      if (h==0)
	return null;
      else
	return getItem(h);
    }

  private native long cpp_insert_after();

  public Item insert_after()
    {
      long h = cpp_insert_after();
      if (h==0)
	return null;
      else
	return getItem(h);
    }

  private native long cpp_insert_before();

  public Item insert_before()
    {
      long h = cpp_insert_before();
      if (h==0)
	return null;
      else
	return getItem(h);
    }

  private native static boolean initialise_cpp();
  private native static boolean finalise_cpp();
  private native boolean create_cpp_item(long handle);
  private native boolean destroy_cpp_item();

  static {
    System.loadLibrary("estjava");
    if (!initialise_cpp())
	throw new ExceptionInInitializerError("Item C++ fails");
  }
}
