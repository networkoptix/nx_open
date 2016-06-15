
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

import cstr.est.*;

public class ScrollWindow extends Frame
			  implements ActionListener, WindowListener
{
  ScrollPane scroller;
  Canvas canvas;
  
  private void init(String title, Component contents, int width, int height)
    {
      setTitle(title);
      setSize(width, height);

      scroller = new ScrollPane(ScrollPane.SCROLLBARS_ALWAYS);
      scroller.add(contents);
      scroller.doLayout();

      MenuBar mb = new MenuBar();
      setMenuBar(mb);

      Menu cMenu = new Menu("Control", true);
      mb.add(cMenu);

      MenuItem mItem = new MenuItem("Quit");
      mItem.addActionListener(this);
      cMenu.add(mItem);

      addWindowListener(this);

      setLayout(new BorderLayout());
      add("Center", scroller);
      pack();
    }

  public ScrollWindow(String name, Component contents)
    {
      super();
      init(name, contents, 500, 500);
    }

  public ScrollWindow(String name, Component contents, int width, int height)
    {
      super();
      init(name, contents, width, height);
    }

  public void quit()
    {
      dispose();
    }

  public void quit(String s)
    {
      dispose();
    }

  public void actionPerformed(ActionEvent event)
    {
      quit("action");
    }

  public void windowClosing(WindowEvent event)
    {
      quit("close");
    }

  public void windowOpened(WindowEvent event)
    {
    }

  public void windowClosed(WindowEvent event)
    {
    }

  public void windowIconified(WindowEvent event)
    {
    }

  public void windowDeiconified(WindowEvent event)
    {
    }

  public void windowActivated(WindowEvent event)
    {
    }

  public void windowDeactivated(WindowEvent event)
    {
    }

}
