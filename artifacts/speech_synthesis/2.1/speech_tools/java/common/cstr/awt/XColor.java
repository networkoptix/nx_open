
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
 //                    Date: Thu Apr  9 1998                               \\
 //  --------------------------------------------------------------------  \\
 //  Extend awt.Color to understand X11 color names.                       \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.awt ;

import java.lang.*;
import java.io.*;
import java.util.*;
import java.awt.*;

public class XColor extends Color
{
  protected static String rgbFile;
  protected static Hashtable colors = new Hashtable(100);
  protected static Reader rgbStream;
  protected static StreamTokenizer rgbTok;

  protected String name;

    static 
    {
      rgbFile = System.getProperty("est.rgbFile");
    }

  public XColor(float r, float g, float b)
    {
      super(r,g,b);
      name=null;
    }

  public XColor(int rgb)
    {
      super(rgb);
      name=null;
    }

  public XColor(int r, int g, int b)
    {
      super(r,g,b);
      name=null;
    }

  public XColor(String n)
    {
      super(x11ColorRGB(n));
      name=n;
    }

  public XColor(String n, int i)
    {
      super(x11ColorRGBThrow(n));
      name=n;
    }

  public static int x11ColorRGBThrow(String name)
        throws IllegalArgumentException
    {
	int c = x11ColorRGB(name);
	if (c<0)
	    throw new IllegalArgumentException("Can't find color '"+name+"'");
	return c;
    }

  public static int x11ColorRGB(String name)
    {

      name = name.toLowerCase();

      Integer rgbI = (Integer)colors.get(name);

      if (rgbI != null)
	return rgbI.intValue();

      if (name.startsWith("#"))
	try {
	  return Integer.parseInt(name.substring(1), 16);
	} catch (NumberFormatException ex) {
	  System.err.println("Can't parse color '"+name+"'");
	  return 0;
	};

      if (rgbStream == null)
	try {
	  rgbStream = new FileReader(rgbFile);
	  rgbTok = new StreamTokenizer(rgbStream);
	  rgbTok.parseNumbers();
	  rgbTok.ordinaryChar('\n');
	  
	} catch (FileNotFoundException ex) {
	  System.err.println("can't open '" + rgbFile + "' " + ex.getMessage());
	}
      
      int r=0;
      int g=0;
      int b=0;

      String n="";

      
      try {
      while (1==1)
	{
	  int t = rgbTok.nextToken();

	  if (t == rgbTok.TT_EOF)
	    break;

	  if (t == '\n')
	    continue;

	  if ( t == '!')
	    {
	      while (t != '\n' && t != rgbTok.TT_EOF)
		t = rgbTok.nextToken();
	      continue;
	    }

	  if ( t == rgbTok.TT_NUMBER)
	      r = (int)rgbTok.nval;

	  t = rgbTok.nextToken();
	  if ( t == rgbTok.TT_NUMBER)
	      g = (int)rgbTok.nval;

	  t = rgbTok.nextToken();
	  if ( t == rgbTok.TT_NUMBER)
	      b = (int)rgbTok.nval;

	  n="";
	  while ((t = rgbTok.nextToken()) != '\n' && t != rgbTok.TT_EOF)
	    {
	      if (!n.equals(""))
		n = n + " ";

	      n = n + rgbTok.sval;
	    }

	  int rgb = (r<<16)|(g<<8)|(b);
	  colors.put(n.toLowerCase(), new Integer(rgb));
	  
	  if (name.equals(n.toLowerCase()))
	      return rgb;
	}
      } catch (IOException ex) {
	System.err.println(ex.getMessage());
      }
	

      System.err.println("Can't find color '"+name+"'");
      return -1;
    }

  public static String getName(Color c)
    {
      return "#"+Integer.toString(c.getRGB(), 16);
    }

  public String getName()
    {
      if (name != null)
	return name;
      return getName(this);
    }
  
}
