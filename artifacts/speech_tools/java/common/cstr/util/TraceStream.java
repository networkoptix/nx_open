
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
 //  A single stream of trace output.                                      \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\

package cstr.util ;

import java.lang.*;
import java.util.*;
import java.awt.*;

// Record for a single stream

public final class TraceStream
{
  protected String name;
  protected boolean off;
  protected boolean bol;
  protected Vector saved;

  public TraceStream(String n)
    {
      this(n, false, false);
    }

  public TraceStream(String n, boolean o, boolean s)
    {
      name=n;
      setOn(o);
      setSave(s);
      bol=true;
      Trace.streams.put(name, this);
    }

  public void setOn(boolean o)
    {
      off=!o;
    }

  public void setSave(boolean s)
    {
      if (s && saved==null)
	saved=new Vector(5);

      if (!s && saved != null)
	{
	  Vector pending=saved;
	  saved=null;
	  for(int i=0; i<pending.size(); i++)
	    print((String)pending.elementAt(i));
	}
    }

  public void print(String s)
    {
      if (off) return;

      if (bol)
	{
	  bol=false;
	  print(name);
	  print(": ");
	}

      if (saved!=null)
	saved.addElement(s);
      else
	{
	  System.out.print(s);
	  if (s.endsWith("\n"))
	    bol=true;
	}
    }

  public void nl()
    {
      if (Trace.enabled)
	  print("\n");
    }
    
  public void p(String what)
    {
      if (Trace.enabled)
	  print(what);
    }

  public void pl(String what)
    {
      if (Trace.enabled)
	{
	  print(what);
	  nl();
	}
    }

}

