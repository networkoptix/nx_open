
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
 //                    Date: Mon Sep 22 1997                               \\
 //  --------------------------------------------------------------------  \\
 //  A class which implements the image producer interface using a Wave    \\
 //  as the source of the image.                                           \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\

package cstr.est.awt;

import java.util.*;
import java.lang.*;
import java.awt.*;
import java.awt.image.*;

import cstr.est.*;

public class WaveImageSource implements ImageProducer
{
  static ColorModel cmodel;

  static {
    cmodel = new IndexColorModel(24, 5, new byte[] 
				 { 
				-1, -1, -1,
				-32, 110, 70,
				-64, 100, 60,
				-96, 90, 50,
				-127, 80, 40
				}, 
				 0, false); 
  }

  Wave wave;
  int channel;
  int x;
  int chunk;
  int width;
  int height;
  int amplitude;

  Vector consumers;

  public WaveImageSource(Wave w, int c, 
			 int xoff, int xlen,
			 int hsize, int vsize)
    {
      wave = w;
      channel=c;
      x = xoff;
      chunk = xlen;
      width = hsize;
      height = vsize;
      amplitude =  w.amplitude(channel) + 100;
      
      consumers = new Vector(1);
    }

  public WaveImageSource(Wave w, int vsize)
    {
      this(w,  0, 0, w.num_samples(), w.num_samples(), vsize);
    }

  public WaveImageSource(Wave w)
    {
      this(w, 0, 0, w.num_samples(), w.num_samples(), 150);
    }

  public void addConsumer(ImageConsumer ic)
    {
      if (!consumers.contains(ic))
	consumers.addElement(ic);
    }

  public boolean isConsumer(ImageConsumer ic)
    {
      return consumers.contains(ic);
    }

  public void removeConsumer(ImageConsumer ic)
    {
      consumers.removeElement(ic);
    }

  public void startProduction(ImageConsumer ic)
    {
      addConsumer(ic);
      produce();
    }

  public void requestTopDownLeftRightResend(ImageConsumer ic)
    {
      produce();
    }

  private void sendHints(int hints)
    {
      int i;
      for(i=0; i<consumers.size(); i++)
	{
	  ImageConsumer ic = (ImageConsumer)consumers.elementAt(i);
	  if (ic != null)
	    ic.setHints(hints);
	}
    }

  private void sendColorModel()
    {
      int i;
      for(i=0; i<consumers.size(); i++)
	{
	  ImageConsumer ic = (ImageConsumer)consumers.elementAt(i);
	  if (ic != null)
	    ic.setColorModel(cmodel);
	}
    }

  private void sendDimensions()
    {
      int i;
      for(i=0; i<consumers.size(); i++)
	{
	  ImageConsumer ic = (ImageConsumer)consumers.elementAt(i);
	  if (ic != null)
	    ic.setDimensions(chunk, height);
	}
    }

  private void sendLine(byte [] line, int l, int n)
    {
      int i;
      for(i=0; i<consumers.size(); i++)
	{
	  ImageConsumer ic = (ImageConsumer)consumers.elementAt(i);
	  if (ic != null)
	      ic.setPixels(0, l, chunk, n, cmodel, line, 0, chunk);
	}
    }

  private void sendComplete(int status)
    {
      int i;
      for(i=0; i<consumers.size(); i++)
	{
	  ImageConsumer ic = (ImageConsumer)consumers.elementAt(i);
	  if (ic != null)
	    ic.imageComplete(status);
	}
    }

  protected void produce()
    {
      System.out.println("start prod");

      sendHints(ImageConsumer.SINGLEFRAME 
		| ImageConsumer.SINGLEPASS 
		| ImageConsumer.TOPDOWNLEFTRIGHT);

      sendColorModel();
      sendDimensions();


      int nlines=30;
      byte [] line = new byte[chunk*nlines];

      System.out.println("start send");
      for(int l=0; l < height ; l+=nlines)
	{
	  int n = height-l>nlines?nlines:height-l;
	  System.out.println("g");
	  wave.cpp_getScanlines(channel, line, l, n, 
				x, chunk, 
				width, height, 
				amplitude);
	  System.out.println("s");
	  sendLine(line, l, n);
	}

      System.out.println("end send");
      sendComplete(ImageConsumer.STATICIMAGEDONE);

      System.out.println("end prod");
    }
}


