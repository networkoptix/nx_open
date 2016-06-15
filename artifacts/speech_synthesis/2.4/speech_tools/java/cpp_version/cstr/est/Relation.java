
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

package cstr.est ;

import java.lang.*;
import java.util.*;
import java.awt.*;

import java.io.*;

import cstr.util.*;

public class Relation
  implements Featured
{
  private long cpp_handle;
  private Utterance utterance;
  private boolean mine;

  public Relation()
    {
      this(0L, null, true);
    }

  public Relation(long handle)
    {
      this(handle, null, false);
    }

  public Relation(long handle, Utterance u)
    {
      this(handle, u, false);
    }

  Relation(long handle, Utterance u, boolean m)
    {
      create_cpp_relation(handle);
      utterance=u;
      mine=m;
    }

  protected void finalize() throws Throwable
    {
      if (mine)
	destroy_cpp_relation();
      super.finalize();
    }

  protected final Item getItem(long handle)
    {
      Item i;
      if (utterance != null)
	  i = utterance.getItem(handle);
      else
	i = new Item(handle);
      return i;
    }

  private native String cpp_name();

  public String name()
    {
      return cpp_name();
    }

  public String getName()
    {
      return cpp_name();
    }

  public long getHandle()
    {
      return cpp_handle;
    }

  public boolean equals(Object o)
    {
      return o instanceof Relation && ((Relation)o).cpp_handle == cpp_handle;
    }

  private native String cpp_type();
  
  public String type()
    {
      return cpp_type();
    }

  private native String cpp_getFeature(String n);

  public String getFeature(String n)
    {
      if (n.equals("_NAME_"))
	return getName();
      else
	return cpp_getFeature(n);
    }

  public String getS(String n)
    {
      if (n.equals("_NAME_"))
	return getName();
      else
	return cpp_getFeature(n);
    }

  public String [] featureNames()
    {
	return featureNames(false, true);
    }

  public String [] featureNames(boolean nodes, boolean leaves)
    {
      Vector names = new Vector();
      Hashtable found = new Hashtable();

      Enumeration is = getElements();

      while (is.hasMoreElements())
	{
	  Item item = (Item)is.nextElement();
	  Item_Content cont = item.getContent();
	  Vector paths = new Vector();
	  
	  cont.getFeatures().getPaths(null, paths, nodes, leaves);
	  
	  for(int i=0; i< paths.size(); i++)
	    {
		String name = (String)paths.elementAt(i);

		if (found.get(name) != null)
		  continue;

		for(int p=0; p<names.size(); p++)
		    {
			int cmp = name.compareTo((String)names.elementAt(p));
			if (cmp>0)
			    continue;
			if (cmp< 0)
			  {
			    names.insertElementAt(name, p);
			    found.put(name,name);
			  }

			name=null;
			break;
		    }
		if (name != null)
		  {
		    names.addElement(name);
		    found.put(name, name);
		  }
	    }
	}

      String [] ns = new String[names.size()];

      for(int i=0; i<names.size(); i++)
	ns[i] = (String)names.elementAt(i);

      return ns;
    }

  public void setUtterance(Utterance u)
    {
      utterance=u;
    }

  public Utterance getUtterance()
    {
      return utterance;
    }

  private native String cpp_load(String filename);

  public void load(String filename) throws FileNotFoundException
    {
      String res = cpp_load(filename);

      if (!res.equals(""))
	  throw new FileNotFoundException(res);

      // findTimePoints();
    }

  private native String cpp_save(String filename, String format);

  public void save(String filename, String format) throws IOException 
    {
      String res = cpp_save(filename, format);

      if (!res.equals(""))
	  throw new IOException(res);
    }

  private native float cpp_getEndTime();

  public float getEndTime()
    {
      return cpp_getEndTime();
    }

  public Enumeration getElements()
    {
      return new RelationEnumeration(this);
    }

  public Enumeration getElements(float from, float to)
    {
      return new RelationEnumeration(this, from, to);
    }

  private native long cpp_head();

  public Item head()
    {
      long h = cpp_head();
      if (h != 0)
	return getItem(h);
      else
	return null;
    }

  private native long cpp_tail();

  public Item tail()
    {
      long h = cpp_tail();
      if (h != 0)
	return getItem(h);
      else
	return null;
    }

  private native long cpp_append();

  public Item append()
    {
	long h = cpp_append();
	if (h != 0)
	    return getItem(h);
	else
	    return null;
    }

 private native void cpp_removeItemList(long itemh);

  public void removeItemList(Item i)
    {
	cpp_removeItemList(i.getHandle());
    }

  private native long cpp_findItem(float time);
  
  public Item findItem(float time)
    {
      long p = cpp_findItem(time);
      return p==0L?null:getItem(p);
    }

  private native static boolean initialise_cpp();
  private native static boolean finalise_cpp();
  private native boolean create_cpp_relation(long handle);
  private native boolean destroy_cpp_relation();

  static {
    System.loadLibrary("estjava");
    if (!initialise_cpp())
	throw new ExceptionInInitializerError("Relation C++ fails");
  }


  static class RelationEnumeration implements Enumeration
  {
    private Relation relation;
    private Item current;
    private float endTime;

    public RelationEnumeration(Relation st)
      {
	relation = st;
	current = st.head();
	endTime = (float)-1.0;
      }

    public RelationEnumeration(Relation st, float from, float to)
      {
	relation = st;
	current = st.findItem(from);
	endTime = to;
      }

    public boolean hasMoreElements()
      {
	return current != null;
      }

    public Object nextElement()
      {
	Item n = current;

	current = n.down();

	if (current == null)
	    current = n.next();

	if (current == null)
	  {
	    current=n;
	    while (current.prev()!=null)
	      current=current.prev();
	    current = current.up();
	    while (current != null && current.next()==null)
	      {
		while (current.prev()!=null)
		  current=current.prev();
		current=current.up();
	      }
	    if (current != null)
	      current = current.next();
	  }

	if (current != null && endTime >= 0 && current.getEndTime() > endTime)
	  current = null;

	return n;
      }
  }
}
