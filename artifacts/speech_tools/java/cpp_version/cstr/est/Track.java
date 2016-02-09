
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
 //  Wrapper around the EST_Track class.                                   \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.est;

import java.util.*;
import java.lang.*;
import java.io.*;

public class Track
{
  private long cpp_handle;

  public Track()
    {
      create_cpp_track();
    }

  protected void finalize() throws Throwable
    {
      destroy_cpp_track();
      super.finalize();
    }

  public long getHandle()
    {
      return cpp_handle;
    }

  private native String cpp_name();

  public String name()
    {
      return cpp_name();
    }

  private native String cpp_load(String filename);

  public void load(File filename) throws FileNotFoundException
    {
      String res = cpp_load(filename.getPath());

      if (!res.equals(""))
	  throw new FileNotFoundException(res);
    }

  private native String cpp_save(String filename, String format, float start, float end);

  public void save(File filename, TrackFileFormat format, float start, float end)
		throws IOException
    {
      String res = cpp_save(filename.getPath(), format.toString(), start, end);
      if (!res.equals(""))
	throw new IOException(res);
    }

  public void save(File filename, float start, float end)
		throws IOException
    {
      String res = cpp_save(filename.getPath(), 
			    TrackFileFormat.EST_BINARY.toString(), 
			    start, end);
      if (!res.equals(""))
	throw new IOException(res);
    }

  private native int cpp_num_frames();

  public int num_frames()
    {
      return cpp_num_frames();
    }

  private native int cpp_num_channels();

  public int num_channels()
    {
      return cpp_num_channels();
    }

  private native String cpp_channelName(int i);

  public String channelName(int i)
    {
      return cpp_channelName(i);
    }

  private native int cpp_channelPosition(String n);

  public int channelPosition(String n)
    {
      return cpp_channelPosition(n);
    }

  public native float a(int i, int c);
  public native float a(float t, int c);
  

  private native float cpp_t(int i);
  
  public float t(int i)
    {
      return cpp_t(i);
    }

  private native boolean cpp_val(int i);
  
  public boolean val(int i)
    {
      return cpp_val(i);
    }

  private native int cpp_frameBefore(float time);
  
  public int frameBefore(float time)
    {
      return cpp_frameBefore(time);
    }

  private native int cpp_frameAfter(float time);
  
  public int frameAfter(float time)
    {
      return cpp_frameAfter(time);
    }

  private native int cpp_frameNearest(float time);
  
  public int frameNearest(float time)
    {
      return cpp_frameNearest(time);
    }

  private native float cpp_getEndTime();

  public float getEndTime()
    {
      return cpp_getEndTime();
    }

  private native static boolean initialise_cpp();
  private native static boolean finalise_cpp();
  private native boolean create_cpp_track();
  private native boolean destroy_cpp_track();

  static {
    System.loadLibrary("estjava");
    if (!initialise_cpp())
	throw new ExceptionInInitializerError("Track C++ fails");
  }
}
