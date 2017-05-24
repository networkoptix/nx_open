
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
 //  EST Features mapped to Java.                                          \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\

package cstr.est;

import java.lang.*;
import java.util.*;

import cstr.util.*;

public class Features
  implements Featured
{
  private long cpp_handle;
  private boolean mine;
  private String [] cachedFeatureNames;

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
      // System.out.println("create "+handle+":"+m);
      create_cpp_features(handle);
      mine=m;
    }

  protected void finalize() throws Throwable
    {
      if (mine)
	destroy_cpp_features();
      super.finalize();
    }

  private native String [] cpp_featureNames();

  public String [] names()
    {
      if (cachedFeatureNames ==null)
	cachedFeatureNames = cpp_featureNames();
      return cachedFeatureNames;
    }

  public void getPaths(String prefix, Vector names, boolean nodes, boolean leaves)
    {
      String [] fns = names();

      // System.err.println("getFeatureNames:"+fns.length);
       for(int i=0; i<fns.length; i++)
	 {
	   Object v = get(fns[i]);

	   //	   System.err.println("="+fns[i]+":"+v+":"+v.getClass());
	   if (v==null)
	     continue;
	   else if (v instanceof cstr.est.Features)
	     {
	       //System.err.println("Features");
	       
	       if ( prefix==null )
		   {
		       if (nodes)
			   names.addElement(fns[i]);
		       ((Features)v).getPaths(fns[i], names, nodes, leaves);
		   }
			   
	       else
		   {
		       if (nodes)
			   names.addElement(prefix + "." + fns[i]);
		       ((Features)v).getPaths(prefix + "." + fns[i], names, nodes, leaves);
		   }
	     }
	   else
	     {
	       // System.err.println("Other:"+v.getClass());
	       if ( prefix==null )
		   {
		       if (leaves)
			   names.addElement(fns[i]);
		   }
	       else
		   {
		       if (leaves)
			   names.addElement(prefix + "." + fns[i]);
		   }
	     }
	 }
    }

  private native boolean cpp_present(String n);

  public boolean present(String n)
    {
      return cpp_present(n);
    }

  private native String cpp_getS(String n, String def);
  
  public String getS(String n)
    {
      return cpp_getS(n, "");
    }

  public String getS(String n, String def)
    {
      return cpp_getS(n, def);
    }

  public String getFeature(String n)
    {
      return cpp_getS(n, "");
    }

  private native float cpp_getF(String n, float def);
  
  public float getF(String n)
    {
      return cpp_getF(n,(float)0.0);
    }

  public float getF(String n, float def)
    {
      return cpp_getF(n,def);
    }

  private native Object cpp_get(String n, Object def);
  
  public Object get(String n)
    {
      return cpp_get(n,null);
    }

  public Object get(String n, Object def)
    {
      return cpp_get(n,def);
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

  public final int hashCode()
    {
      return (int)cpp_handle;
    }
  
  public boolean equals(Object i)
    {
      return i instanceof Features && ((Features)i).cpp_handle == cpp_handle;
    }

  private native static boolean initialise_cpp();
  private native static boolean finalise_cpp();
  private native boolean create_cpp_features(long handle);
  private native boolean destroy_cpp_features();

  static {
    System.loadLibrary("estjava");
    if (!initialise_cpp())
	throw new ExceptionInInitializerError("Features C++ fails");
  }

}
