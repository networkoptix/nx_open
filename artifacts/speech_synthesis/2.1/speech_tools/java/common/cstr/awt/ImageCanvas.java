
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
package cstr.awt;

import java.util.*;
import java.lang.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;

public class ImageCanvas extends Canvas
{
  Image image;
  int pwidth, pheight;

  private void init(Image im, double xscale, double yscale)
    {
      pwidth = (int)(im.getWidth(this)*xscale);
      pheight = (int)(im.getHeight(this)*yscale);

      if (xscale != 1.0 || yscale != 1.0)
	{
	  ImageFilter filter =  new ReplicateScaleFilter(pwidth, pheight);

	  image = createImage(new FilteredImageSource(im.getSource(), filter));
	}
      else
	image = im;
    }
  
  public ImageCanvas(Image touse, double xscale, double yscale)
    {
      super();
      
      init(touse, xscale, yscale);
    }

  public ImageCanvas(ImageProducer prod, double xscale, double yscale)
    {
      super();

      init(createImage(prod), xscale, yscale);
    }

  public ImageCanvas(Image touse, double scale)
    {
      super();
      
      init(touse, scale, scale);
    }

  public ImageCanvas(ImageProducer prod, double scale)
    {
      super();

      init(createImage(prod), scale, scale);
    }

  public ImageCanvas(Image touse)
    {
      super();
      
      init(touse, 1.0, 1.0);
    }

  public ImageCanvas(ImageProducer prod)
    {
      super();

      init(createImage(prod), 1.0, 1.0);
    }

  public boolean imageUpdate(Image img,
			     int flags,
			     int x,
			     int y,
			     int w,
			     int h)
    {
      int oh =pheight;
      int ow = pwidth;

      if ((flags & ImageObserver.WIDTH) != 0)
	pwidth = w;
      if ((flags & ImageObserver.HEIGHT) != 0 )
	pheight = h;

      if (pwidth!= ow || pheight != oh)
	  setSize(pwidth, pheight);

      repaint();
      return !((flags & ImageObserver.ALLBITS) != 0);
    }

  public Dimension getPreferredSize()
    {
      return new Dimension(pwidth, pheight);
    }

  public Dimension getMinimumSize()
    {
      return new Dimension(pwidth, pheight);
    }

  public void update(Graphics g)
    {
      paint(g);
    }
  public void paint(Graphics g)
    {
      g.drawImage(image, 0, 0, pwidth, pheight, this);
    }
}
