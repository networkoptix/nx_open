
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
 //  Java wrapper around relations.                                        \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\

package cstr.est.cpp ;

import java.lang.*;
import java.util.*;
import java.awt.*;

import java.io.*;

public class skeleton
{
  private long cpp_handle;
  

  public Skeleton()
    {
      create_cpp_utterance();
    }

  protected void finalize() throws Throwable
    {
      destroy_cpp_utterance();
      super.finalize();
    }

  private native String cpp_name();

  public String name()
    {
      return cpp_name();
    }

  private native String cpp_load(String filename);

  public void load(String filename) throws FileNotFoundException
    {
      String res = cpp_load(filename);

      if (!res.equals(""))
	  throw new FileNotFoundException(res);
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
	throw new ExceptionInInitializerError("Skeleton C++ fails");
  }
}
