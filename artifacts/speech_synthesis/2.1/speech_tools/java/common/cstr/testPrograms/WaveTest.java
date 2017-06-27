
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
 //  Simple test program to see if wave class works.                       \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.testPrograms;

import java.awt.*;
import java.util.*;
import java.awt.event.*;
import java.awt.image.*;
import java.lang.*;
import java.io.*;

import cstr.awt.*;
import cstr.est.*;
import cstr.est.awt.*;


public class WaveTest 
{

  public static void useage(String com)
    {
      System.out.println("Useage: WaveTest FILENAME " + com);
      System.exit(1);
    }

  public static int save(Wave wv, int pos, String[] args) 
    {
      if (args.length  - pos <3)
	  useage("save NEWFILENAME FORMAT");

      try {
	wv.save(args[pos+1], args[pos+2]);
      } catch (IOException e) {
	System.out.println("Save Error: " + e.getMessage());
	System.exit(1);
      }
      return 3;
    }

  public static int display(Wave wv, int pos, String[] args) 
    {
      if (args.length  - pos <1)
	  useage("display");


      ImageProducer prod = new WaveImageSource(wv, 0, 400, 500, 1500, 150);

      ImageCanvas canvas = new ImageCanvas(prod);

      ScrollWindow win = new ScrollWindow(wv.name(), canvas, 500, 250);

      try {
	win.setVisible(true);
      } catch (AWTError e) {
	System.out.println("Exit: " + e.getMessage());
      }

      return 1;
    }

  public static int resample(Wave wv, int pos, String[] args) 
    {
      if (args.length - pos < 2)
	  useage("resample RATE");

      int rate=0;

      try {
	rate = Integer.decode(args[pos+1]).intValue();
      } catch (NumberFormatException e) {
	System.out.println("Sample rate must be integer > 0 not " + args[pos+1]);
	System.exit(1);
      }

      if (rate <= 0)
	{
	  System.out.println("Sample rate must be > 0");
	  System.exit(1);
	}

      wv.resample(rate);

      return 2;
    }

  public static void main(String [] args)
    {
      System.runFinalizersOnExit(true);

      Wave wv = new Wave();

      if (args.length < 1)
	{
	  useage("COMMAND {ARGS...} ...");
	  return;
	}

      try {
	wv.load(args[0]);
      } catch (FileNotFoundException e) {
	System.out.println("Load Error: " + e.getMessage());
	return;
      }

      int pos=1;

      while (pos < args.length)
	{
	  if (args[pos].equals("save"))
	    pos += save(wv, pos, args);
	  else if (args[pos].equals("resample"))
	    pos += resample(wv, pos, args);
	  else if (args[pos].equals("display"))
	    pos += display(wv, pos, args);
	  else 
	    useage("save/resample/display ...");
	}
    }
}

