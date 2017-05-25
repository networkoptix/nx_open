
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
 //    3. Original authors' names are not delete.                         \\
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
 //  A simple java wave class.                                             \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\
package cstr.est ;

import java.lang.*;
import java.util.*;
import java.net.*;
import java.io.*;

import sun.applet.*;

public class Wave
{
  int rate;
  int amplitude_cache=-1;
  short [] data;
  File fn;

  public Wave()
    {
      data=null;
      fn=null;
    }

  public Wave(byte [] bytes)
		    throws UnsupportedEncodingException 
    {
      fn=null;
      if (!parse(bytes))
	{
	  throw new UnsupportedEncodingException("Can't create Wave from this bytestream");
	}
    }

  public Wave(File file)
		    throws UnsupportedEncodingException, IOException
    {
      FileInputStream is = new FileInputStream(file);

      int n=0;
      byte [] buf = new byte[n];

      while (true)
	{
	  byte [] ibuf = new byte[10240];
	  int nr = is.read(ibuf, 0, 10240);
	  if (nr == -1)
	    break;

	  if (n==0)
	    {
	      buf = ibuf;
	      n = nr;
	    }
	  else
	    {
	      byte [] nbuf = new byte[n+nr];
	      for(int i=0; i<n; i++)
		nbuf[i] = buf[i];
	      for(int i=0; i<nr; i++)
		nbuf[i+n] = ibuf[i];
	      buf=nbuf;
	      n += nr;
	    }
	}
      
      if (!parse(buf))
	{
	  throw new UnsupportedEncodingException("Can't create Wave from this bytestream");
	}
    }

  public void finalize()
    {
      if (fn != null)
	fn.delete();
    }

  public String name()
    {
      return toString();
    }

  public void resample(int rate)
    {
    }

  public int amplitude(int c)
    {
      if (amplitude_cache<0)
	for(int i=0; i<data.length; i++)
	  { 
	    short d=data[i];
	    if (d>=0 && d > amplitude_cache)
	      amplitude_cache=d;
	    if (d<0 && -d > amplitude_cache)
	      amplitude_cache=-d;
	  }
      return amplitude_cache;
    }

  public int num_samples()
    {
      return data.length;
    }

  public void cpp_getScanlines(int c,
			       byte[] line, 
			       int lstart, int lnum,  
			       int x, int chunk,
			       int width, int height, 
			       int amplitude
			       )
    {
    }

  private File mkFilename()
    {
      StringBuffer fn= new StringBuffer(50);

      fn.append("/tmp/jsapi_");
      fn.append(this.toString());
      fn.append(".au");

      return new File(fn.toString());
    }

  public boolean parse(byte[] bytes)
    {
      //clip = new AppletAudioClip(bytes);
      fn=mkFilename();

      try {
	FileOutputStream os = new FileOutputStream(fn);
	
	os.write(bytes);
	
	os.close();
      } catch (IOException ex) {
	System.err.println("IO Exception: "+ex.getMessage());
	return false;
      }

      int magic = (bytes[0] <<24) + (bytes[1] <<16) + (bytes[2] <<8) + bytes[3];
      int smagic = (bytes[3] <<24) + (bytes[2] <<16) + (bytes[1] <<8) + bytes[0];

      if (magic == 0x2e736e64)
	{
	  return true;
	}

      return false;
    }

  public void play()
    {
      if (fn != null)
	{
	  try {
	    String command = "na_play "+fn.toString();
	    // System.out.println("play command "+command);
	    Process p = Runtime.getRuntime().exec(command);
	    p.waitFor();
	  } catch (IOException ex) {
	  } catch (InterruptedException ex) {
	  }
	}
    }

  public void stop()
    {
    }

  public void load(String filename) throws FileNotFoundException
    {
      throw new FileNotFoundException("Load Not yet Implemented");
    }

  public void save(String filename, String format) throws IOException 
    {
      throw new IOException("Save Not yet Implemented");
    }

}
