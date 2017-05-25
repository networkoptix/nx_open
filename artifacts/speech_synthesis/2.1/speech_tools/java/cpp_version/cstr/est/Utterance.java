
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
 //                    Date: Friday 12th September 1997                    \\
 //  --------------------------------------------------------------------  \\
 //  Wrapper around the EST_Utterance class.                               \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\

package cstr.est;

import java.util.*;
import java.lang.*;
import java.io.*;

import cstr.util.*;

public class Utterance
{
  private long cpp_handle;
  private LongHash cache;

  public Utterance()
    {
      create_cpp_utterance();
      cache = new LongHash(200);
    }

  protected void finalize() throws Throwable
    {
      destroy_cpp_utterance();
      super.finalize();
    }

  final Item getItem(long handle)
    {
      Item i;
      i = (Item)cache.get(handle);
      if (i==null)
	{
	  i = new Item(handle, this);
	  cache.put(handle, i);
	}
      return i;
    }

  private native int cpp_num_relations();

  public int num_relations()
    {
      return cpp_num_relations();
    }

  private native boolean cpp_has_relation(String name);

  public boolean has_relation(String n)
    {
      return cpp_has_relation(n);
    }

  private native long cpp_relation_n(int n);

  public Relation relation(int n)
    {
      return new Relation(cpp_relation_n(n), this);
    }

  private native long cpp_relation(String name);

  public Relation relation(String n)
    {
      long rel_h = cpp_relation(n);
      if (rel_h==0)
	return null;
      else
	return new Relation(rel_h, this);
    }

  private native long cpp_create_relation(String name);

  public Relation create_relation(String name)
    {
	long rel_h = cpp_create_relation(name);
	if (rel_h==0)
	    return null;
	else
	    return new Relation(rel_h, this);
    }

  private native String cpp_load(String filename);

  public void load(String filename) throws FileNotFoundException
    {
      String res = cpp_load(filename);

      if (!res.equals(""))
	  throw new FileNotFoundException(res);
    }

  private native String cpp_save(String filename, String format);

  public void save(String filename, UtteranceFileFormat format) throws IOException
    {
      String res = cpp_save(filename, format.toString());

      if (!res.equals(""))
	  throw new IOException(res);
    }

  public void save(String filename) throws IOException
    {
      save(filename, UtteranceFileFormat.EST_ASCII);
    }

  private native int cpp_findItem(float time);
  
  public int findItem(float time)
    {
      return cpp_findItem(time);
    }

  private native float cpp_getEndTime();

  public float getEndTime()
    {
      return cpp_getEndTime();
    }

  private native static boolean initialise_cpp();
  private native static boolean finalise_cpp();
  private native boolean create_cpp_utterance();
  private native boolean destroy_cpp_utterance();

  static {
    System.loadLibrary("estjava");
    if (!initialise_cpp())
	throw new ExceptionInInitializerError("Utterance C++ fails");
  }
}
