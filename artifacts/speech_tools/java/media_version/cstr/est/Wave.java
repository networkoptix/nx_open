
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

import javax.media.*;
import javax.media.protocol.*;

class PlayWatcher 
  implements ControllerListener
{
  private boolean finished;

  public PlayWatcher()
    {
      finished=false;
    }

  public synchronized void waitForPlay()
    {
      while (!finished)
	try {
	  this.wait();
	} catch (InterruptedException e) {
	}
    }


  public synchronized void controllerUpdate(ControllerEvent e) 
    {
      Player p = (Player)e.getSourceController();

      if (e instanceof StopEvent)
	{
	  p.removeControllerListener(this);
	  p.deallocate();
	  p.close();

	  finished=true;

	  this.notifyAll();
	}
    }
}

public class Wave 
{
  int amplitude_cache=-1;
  short [] samples;
  // byte [] sbytes;
  int channels;
  int sample_rate;
  File fn;

  public Wave()
    {
      samples=null;
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
      fn=null;

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
	for(int i=0; i<samples.length; i++)
	  { 
	    short d=samples[i];
	    if (d>=0 && d > amplitude_cache)
	      amplitude_cache=d;
	    if (d<0 && -d > amplitude_cache)
	      amplitude_cache=-d;
	  }
      return amplitude_cache;
    }

  public int num_samples()
    {
      return samples.length;
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

  private final int mkint(byte b0, byte b1, byte b2, byte b3)
    {
      int r = (((short)b3) & 255);
      r += (((short)b2) & 255) << 8;
      r += (((short)b1) & 255) << 16;
      r += (((short)b0) & 255) << 24;
      return r;
    }

  private final short mkshort(byte b0, byte b1)
    {
      short r = (short)(((short)b1) & 255);
      r += (short)(((short)b0) & 255) << 8;
      return r;
    }

  public boolean parse(byte[] bytes)
    {
      int magic = (bytes[0] <<24) + (bytes[1] <<16) + (bytes[2] <<8) + bytes[3];
      int smagic = (bytes[3] <<24) + (bytes[2] <<16) + (bytes[1] <<8) + bytes[0];

      if (magic == 0x2e736e64)
	{
	  // Sun snd, shorts, unswapped
	  int hdr_size = mkint(bytes[4], bytes[5], bytes[6], bytes[7]);
	  int hdr_data_size = mkint(bytes[8], bytes[9], bytes[10], bytes[11]);
	  int hdr_encoding = mkint(bytes[12], bytes[13], bytes[14], bytes[15]);
	  int hdr_sample_rate = mkint(bytes[16], bytes[17], bytes[18], bytes[19]);
	  int hdr_channels = mkint(bytes[20], bytes[21], bytes[22], bytes[23]);

	  if (hdr_encoding != 3)
	    return false;

	  sample_rate = hdr_sample_rate;

	  channels = hdr_channels;

	  int pos = hdr_size;

	  int nsamples = (bytes.length-pos)/2/channels;

	  // System.out.println("sr="+sample_rate+" c="+channels+" nsamp="+nsamples);

	  samples = new short[nsamples];
	  //sbytes = new byte[nsamples*2];

	  for(int s=0; s<nsamples; s++, pos+=2)
	    {
	      samples[s] = mkshort(bytes[pos], bytes[pos+1]);
	      //sbytes[s*2] = bytes[pos];
	      //sbytes[s*2+1] = bytes[pos+1];
	    }

	  return true;
	}

      return false;
    }

  public void play()
    {
      playMedia();
    }

  public void playMedia()
    {
      if (samples != null)
      try {
	DataSource source = new WaveDataSource(this);
	
	source.connect();
	Player player = Manager.createPlayer(source);
	
	//System.err.println("content type="+source.getContentType());
	
	PlayWatcher pw = new PlayWatcher();
	
	player.addControllerListener(pw);
	
	// player.prefetch();
	player.start();
	pw.waitForPlay();
      } catch (IOException ex) {
	//System.err.println("Can't play "+fn+": "+ex.getMessage());
	//	  } catch (NoDataSourceException ex) {
	//	    System.err.println("No player for "+fn+": "+ex.getMessage());
      } catch (NoPlayerException ex) {
	System.err.println("No player for "+fn+": "+ex.getMessage());
      }
    }

  public void playFile()
    {
      fn=mkFilename();

      try {
	FileOutputStream os = new FileOutputStream(fn);
	
	//	os.write(bytes);
	
	os.close();
      } catch (IOException ex) {
	System.err.println("IO Exception: "+ex.getMessage());
	return;
      }

      if (fn != null)
	{
	  try {
	    MediaLocator locator = new MediaLocator(new URL("file:"+fn));
	    DataSource source = Manager.createDataSource(locator);
	    Player player = Manager.createPlayer(source);

	    //System.err.println("content type="+source.getContentType());
	    
	    PlayWatcher pw = new PlayWatcher();

	    player.addControllerListener(pw);

	    player.prefetch();
	    player.start();
	    pw.waitForPlay();
	  } catch (IOException ex) {
	    System.err.println("Can't play "+fn+": "+ex.getMessage());
	  } catch (NoDataSourceException ex) {
	    System.err.println("No player for "+fn+": "+ex.getMessage());
	  } catch (NoPlayerException ex) {
	    System.err.println("No player for "+fn+": "+ex.getMessage());
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
