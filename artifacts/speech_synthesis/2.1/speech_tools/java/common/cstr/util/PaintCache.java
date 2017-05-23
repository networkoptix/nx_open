
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
 //  An abstration which stands between a component and the AWT to cache   \\
 //  the information painted in an array of images.                        \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.util ;

import java.lang.*;
import java.util.*;
import java.awt.*;
import java.awt.image.*;

public class PaintCache
{
  protected Image [] chunks;
  protected Thread [] threads;
  protected int chunkSize;
  protected int numChunks;
  protected Painter painter;
  protected Component comp;
  protected int width;
  protected int height;
  protected int running;
  protected int topPriority;
  Image dummy;

  public PaintCache(Component c, Painter p)
    {
      this(c, p, 400);
    }

  public PaintCache(Component c, Painter p, int cs)
    {
      comp=c;
      painter=p;
      chunkSize=cs;
      chunks=null;
      running=0;
      topPriority=Thread.currentThread().getPriority()-1;
    }

  protected void finalize()
    {
      if (threads!=null)
	for(int i=0; i<threads.length; i++)
	  if (threads[i]!= null)
	    threads[i].stop();
    }

  public synchronized void updated(int w, int h)
    {
      if (w != width || h != height)
	{
	  width = w;
	  height = h;

	  if (threads!=null)
	    for(int i=0; i<threads.length; i++)
	      if (threads[i]!= null)
		threads[i].stop();

	  running=0;

	  numChunks = w / chunkSize+1;
	  if (numChunks*chunkSize < w)
	    numChunks++;

	  chunks = new Image[numChunks+1];
	  threads = new Thread[numChunks+1];

	  dummy = comp.createImage(chunkSize, height);
	  Graphics ig = dummy.getGraphics();

	  ig.setColor(Color.red);

	  ig.drawLine(0,0,chunkSize,height);
	  ig.drawLine(0,height,chunkSize,0);
	}
    }

  protected synchronized Image dummyImage()
    {
      return dummy;
    }

  protected synchronized void dumpP()
    {
      System.out.print("Threads: ");
      for(int i=0; i<threads.length; i++)
	if (threads[i]!= null && threads[i].isAlive())
	  System.out.print(threads[i].getPriority());
      else
	System.out.print("_");
      System.out.println("");
    }

  public synchronized void zap(int i)
    {
      if (i<threads.length && threads[i] == Thread.currentThread())
	{
	  int op=threads[i].getPriority();
	  threads[i]=null;
	  int maxi=-1, max=0;
	  
	  for(int ti=0; ti<threads.length; ti++)
	    if (threads[ti]!= null && threads[ti].isAlive())
	      {
		int p = threads[ti].getPriority();
		//		if (p>Thread.MIN_PRIORITY && p < op)
		//threads[ti].setPriority(p+1);
		if (p>max)
		  {
		    max=p;
		    maxi=ti;
		  }
	      }
	  if (maxi>=0)
	    {
	      // System.out.println("top="+maxi+"="+topPriority);
	      threads[maxi].setPriority(topPriority);
	    }
	      
	  // dumpP();
	}
    }

  protected synchronized void checkChunk(int i, Graphics g)
    {
      if (i<chunks.length)
	{
	  if (chunks[i] == null)
	    {
	      if (painter instanceof ImagePainter)
		{
		  ImagePainter ipainter = (ImagePainter)painter;

		  chunks[i] = dummy;

		  Rectangle r = new Rectangle(i*chunkSize,  0, 
					      chunkSize, height);

		  Thread pt = new Thread(new IPaintingThread(i,
							    ipainter,
							    r,
							    this,
							    g,
							    chunkSize, 
							    height));
	  

		  threads[i]=pt;
		  pt.start();
		  pt.setPriority(Thread.MIN_PRIORITY);
		}
	      else
		{
		  PaintPainter ppainter = (PaintPainter)painter;
		  chunks[i] = comp.createImage(chunkSize, height);
	  
		  Graphics ig = chunks[i].getGraphics();

		  ig.setColor(Color.red);

		  ig.drawLine(0,0,chunkSize,height);
		  ig.drawLine(0,height,chunkSize,0);

		  ig.setColor(g.getColor());

		  Rectangle r = new Rectangle(i*chunkSize,  0, chunkSize, height);

		  Thread pt = new Thread(new PaintingThread(i,
							    ppainter,
							    r,
							    ig,
							    this,
							    g,
							    chunkSize, 
							    height));
	  

		  threads[i]=pt;
		  pt.start();
		  pt.setPriority(Thread.MIN_PRIORITY);
		}
	    }
	  else if (threads[i] != null && threads[i].isAlive())
	    {
	      for(int ti=0; ti<threads.length; ti++)
		{
		  Thread t = threads[ti];
		  if (t != null && t.isAlive())
		    {
		      int p;
		      if (i!=ti)
			{
			  int op = t.getPriority();
			if (op> Thread.MIN_PRIORITY)
			  //			  p= topPriority-1;
			  p= Thread.MIN_PRIORITY+1;
			else
			  p =Thread.MIN_PRIORITY;
			}
		      else
			p = topPriority;
		      //			p = Thread.MAX_PRIORITY;
		      
		      if (p>=Thread.MIN_PRIORITY)
			  t.setPriority(p);
		    }
		}
	      // dumpP();
	    }
	}
    }

  public void paint(Graphics g)
    {
      Rectangle r = g.getClipBounds();
      
      int c1 = r.x / chunkSize;
      int cn = (r.x+r.width) / chunkSize;

      if (cn >= chunks.length)
	cn = chunks.length-1;

      if ( c1 == cn)
	{
	  checkChunk(c1, g);
	  int ix = r.x % chunkSize;
	  g.drawImage(chunks[c1],
		      r.x, r.y, r.x+r.width, r.y+r.height,
		      ix, r.y,  ix+r.width, r.y+r.height,
		      comp);
	}
      else
	{
	  int ix = r.x % chunkSize;
	  int wx = r.x;
	  int w = chunkSize - ix;
	  checkChunk(c1, g);
	  g.drawImage(chunks[c1],
		      wx, r.y, wx+w, r.y+r.height,
		      ix, r.y,  ix+w, r.y+r.height,
		      comp);

	  for(int c = c1+1; c < cn; c++)
	    {
	      ix = 0;
	      wx = c * chunkSize;
	      w = chunkSize;

	      checkChunk(c, g);
	      g.drawImage(chunks[c],
			  wx, r.y, wx+w, r.y+r.height,
			  ix, r.y,  ix+w, r.y+r.height,
			  comp);
	    }

	  ix = 0;
	  wx = cn * chunkSize;
	  w = (r.x + r.width) % chunkSize;

	  checkChunk(cn, g);
	  g.drawImage(chunks[cn],
		      wx, r.y, wx+w, r.y+r.height,
		      ix, r.y,  ix+w, r.y+r.height,
		      comp);
	}

    }

  protected synchronized void blit(Graphics g, int i)
    {
      g.setClip(0,0, width, height);
      if (i<chunks.length)
	g.drawImage(chunks[i],
		    i*chunkSize, 0, 
		    Color.green,
		    null);
    }
      
  class PaintingThread implements Runnable
  {
    int index;
    PaintPainter painter;
    Rectangle rect;
    Graphics ig;
    PaintCache parent;
    Graphics g;
    int w,h;

    public PaintingThread(int i,
			  PaintPainter i_painter,
			  Rectangle i_rect,
			  Graphics i_ig,
			  PaintCache i_parent,
			  Graphics i_g,
			  int i_w,
			  int i_h)
      {
	index=i;
	painter=i_painter;
	rect=i_rect;
	ig=i_ig.create();
	parent=i_parent;
	g=i_g.create();
	w=i_w;
	h=i_h;
      }

    public void run()
      {
	// System.out.println("start "+rect.x);
	painter.real_paint(ig, rect);
	Thread.currentThread().setPriority(Thread.MAX_PRIORITY);
	parent.zap(index);
	parent.blit(g, index);
	// System.out.println("end "+rect.x);
      }
  }

 class IPaintingThread implements Runnable
 {
   int index;
   ImagePainter painter;
   Rectangle rect;
   PaintCache parent;
   Graphics g;
   int w,h;

   public IPaintingThread(int i,
			  ImagePainter i_painter,
			  Rectangle i_rect,
			  PaintCache i_parent,
			  Graphics i_g,
			  int i_w,
			  int i_h)
     {
       index=i;
       painter=i_painter;
       rect=i_rect;
       parent=i_parent;
       g=i_g.create();
       w=i_w;
       h=i_h;
     }

   public void run()
     {
       ImageProducer src = painter.make_image(rect);

       if (src==null)
	   chunks[index] = parent.dummyImage();
       else
	   chunks[index] = comp.createImage(src);
       Thread.currentThread().setPriority(Thread.MAX_PRIORITY);
       parent.zap(index);
       parent.blit(g, index);
       // System.out.println("end "+rect.x);
     }
 }

}




