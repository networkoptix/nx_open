
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
 //  Wrapper around the EST_Wave class. This is pretty horrible in that    \\
 //  it uses a long to hold a pointer to a wave. There must be a better    \\
 //  way.                                                                  \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\

package cstr.est;

import java.util.*;
import java.lang.*;
import java.io.*;

public class Wave 
{
  long cpp_handle;
  int [] amplitudes;

  public Wave()
    {
      create_cpp_wave();
    }

  

  public Wave(byte [] bytes)
		    throws UnsupportedEncodingException 
    {
      if (!parse(bytes))
	{
	  throw new UnsupportedEncodingException("Can't create Wave from this bytestream");
	}
    }

  public Wave(File file)
		    throws UnsupportedEncodingException, IOException
    {
      throw new IOException("Can't load from files yet");
    }

  protected void finalize() throws Throwable
    {
      destroy_cpp_wave();
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

  private native void cpp_setName(String name);

  public void setName(String name)
    {
      cpp_setName(name);
    }

  public String getName()
    {
      return cpp_name();
    }

  private native String cpp_load(String filename);

  public void load(String filename) throws FileNotFoundException
    {
      String res = cpp_load(filename);

      if (!res.equals(""))
	  throw new FileNotFoundException(res);

      amplitudes = new int[num_channels()];
    }

  private native String cpp_save(String filename, String format);

  public void save(String filename, String format) throws IOException 
    {
      String res = cpp_save(filename, format);

      if (!res.equals(""))
	  throw new IOException(res);
    }

  private native void cpp_resample(int rate);


  public void resample(int rate)
    {
      cpp_resample(rate);
    }

  private static native void cpp_set_play_ops(String protocol,
					      String command,
					      String server);

  public static void set_play_ops(String protocol,
				  String command,
				  String server)
    {
      cpp_set_play_ops(protocol, command, server);
    }

  private native void cpp_play(float start, float end);

  public void play(float start, float end)
    {
      System.out.println("play st");
      if (end > start)
	cpp_play(start, end);
      System.out.println("play end");
    }

  private native void cpp_play_all();

  public void play()
    {
	cpp_play_all();
    }


  private native int cpp_num_samples();

  public int num_samples()
    {
      return cpp_num_samples();
    }

  private native int cpp_num_channels();

  public int num_channels()
    {
      return cpp_num_channels();
    }

  private native int cpp_sample_rate();

  public int sample_rate()
    {
      return cpp_sample_rate();
    }

  private native int cpp_amplitude(int c);

  public int amplitude(int c)
    {
      if (amplitudes[c] > 0)
	return amplitudes[c];

      int a =  cpp_amplitude(c);

      amplitudes[c] = a;
      return a;
    }

  public native int a(int x, int c);
  public native int a(float t, int c);

  native public void cpp_getScanlines(int c,
				      byte[] line, 
				      int lstart, int lnum,  
				      int x, int chunk,
				      int width, int height, 
				      int amplitude
				      );

  public boolean parse(byte[] bytes)
    {
      return false;
    }

  native public final int cpp_getMin(int c, int x1, int x2);
  native public final int cpp_getMax(int c, int x1, int x2);

  private native static boolean initialise_cpp();
  private native static boolean finalise_cpp();
  private native boolean create_cpp_wave();
  private native boolean destroy_cpp_wave();

  static {
    System.loadLibrary("estjava");
    if (!initialise_cpp())
	throw new ExceptionInInitializerError("Wave C++ fails");
  }
}
