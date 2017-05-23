
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
 //  Simple Trace package. Multiple tracing streams which can be turned    \\
 //  on and off.                                                           \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.util ;

import java.lang.*;
import java.util.*;
import java.awt.*;

public final class Trace
{
  protected static Hashtable streams= new Hashtable(5);
  protected static boolean enabled=false;
  protected static TraceStream def_st;
  
  static {
    init("Trace");
    def_st=(TraceStream)streams.get("Trace");
    def_st.setOn(true);
  }

  public static void init(String which)
    {
      StringTokenizer tok = new StringTokenizer(which, ",");
      while (tok.hasMoreElements())
	{
	  String name=tok.nextToken();
	  streams.put(name, new TraceStream(name, false, false));
	}
    }

  public static  void setEnable(boolean e)
    {
      enabled=e;
    }

  public static  void setOn(String which, boolean o)
    {
      TraceStream st=(TraceStream)streams.get(which);
      if (st!=null)
	{
	  st.setOn(o);
	  if (o)
	    enabled=true;
	}
      else
	{
	  pl("Unknown Trace Stream "+which);
	}
    }

  public static  void setOn(boolean o)
    {
      def_st.setOn(o);
      if (o)
	enabled=true;
    }

  public static  void setSave(String which, boolean s)
    {
      TraceStream st=(TraceStream)streams.get(which);
      if (st!=null)
	st.setSave(s);
      else
	{
	  p("Unknown Trace Stream "+which);
	  nl();
	}
    }

  public static  void setSave(boolean s)
    {
      def_st.setSave(s);
    }

  public static  void p(String which, String what)
    {
      if (enabled)
	{
	  TraceStream st=(TraceStream)streams.get(which);
	  if (st==null)
	    st = def_st;
	  st.p(what);
	}
    }

  public static  void pl(String which, String what)
    {
      if (enabled)
	{
	  TraceStream st=(TraceStream)streams.get(which);
	  if (st==null)
	    st = def_st;
	  st.pl(what);
	}
    }

  public static  void nl(String which)
    {
      if (enabled)
	{
	  TraceStream st=(TraceStream)streams.get(which);
	  if (st==null)
	    st = def_st;
	  st.nl();
	}
    }

  public static  void p(String what)
    {
      def_st.print(what);
    }

  public static  void pl(String what)
    {
      def_st.pl(what);
    }

  public static  void nl()
    {
	def_st.nl();
    }


}

