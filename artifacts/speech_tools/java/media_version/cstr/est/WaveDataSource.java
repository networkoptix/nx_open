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
 //  Connect a wave to the media API as a dource of sound to play.         \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.est ;

import java.lang.*;
import java.util.*;
import java.net.*;
import java.io.*;

import javax.media.*;
import javax.media.protocol.*;

class WavePullSourceStream implements PullSourceStream
{
  ContentDescriptor cd;
  Wave wave;
  byte [] header;
  int hpos;
  int pos;
  int length;

  public WavePullSourceStream(Wave wv)
    {
      wave=wv;
      pos=-1;
      hpos=0;
      cd = new ContentDescriptor("audio.basic");
      header = new byte[24];
      fill_in_header(header, 0);
    }

  public ContentDescriptor getContentDescriptor()
    {
      return cd;
    }

  public long getContentLength()
    {
      return 24L + wave.samples.length*2;
    }

 public boolean endOfStream()
    {
      return pos >= wave.samples.length;
    }
  

  public boolean willReadBlock()
    {
      return false;
    }

  private final void insert_int(byte b[], int p, int v)
    {
      b[p++] = (byte)((v >>24)&255);
      b[p++] = (byte)((v >>16)&255);
      b[p++] = (byte)((v >>8)&255);
      b[p++] = (byte)((v)&255);
    }

  private int fill_in_header(byte b[], int p)
    {
      insert_int(b, p, 0x2e736e64);
      insert_int(b, p+4, 24);
      insert_int(b, p+8, wave.samples.length*2);
      insert_int(b, p+12, 3);
      insert_int(b, p+16, wave.sample_rate);
      insert_int(b, p+20, wave.channels);
      return 24;
    }

  public int read(byte buffer[],
		  int offset,
		  int length) 
    {
      int n=0;

      //      System.out.println("read o="+offset+" l="+length);

      if (pos == -2)
	return -1;

      if (pos < 0)
	{
	  n = header.length;
	  if (n > length)
	    n = length;
	  for(int b=0; b<n; b++)
	    buffer[offset++] = header[b+hpos];
	  hpos += n;
	  if (hpos >= header.length)
	    pos=0;
	  return n;
	}

      
      int nsamp = length/2;

      // System.out.println("\tnsamp="+nsamp+" pos="+pos+" len="+wave.samples.length);

      if (nsamp > wave.samples.length-pos)
	nsamp = wave.samples.length-pos;

      // System.out.println("\tnsamp="+nsamp);
      for(int s=0; s<nsamp; s++)
	{
	  short samp = wave.samples[s+pos];
	  buffer[offset++] = (byte)((samp>>8)&255);
	  buffer[offset++] = (byte)(samp&255);
		  // buffer[offset++] = wave.sbytes[(s+pos)*2];
		  //	  buffer[offset++] = wave.sbytes[(s+pos)*2+1];
	}

      if (nsamp==0)
	pos =-2;
      else
	pos += nsamp;
      
      return  nsamp*2;
    }

   public Object[] getControls()
    {
      return new Object[0];
    }

   public Object getControl(String controlType)
    {
      return null;
    }

}


public class WaveDataSource extends PullDataSource
{
  MediaLocator locator;
  Wave wave;
  WavePullSourceStream stream;

  public WaveDataSource(Wave wv)
    {
      super();
      wave=wv;
    }

  public void connect()
    {
      stream = new WavePullSourceStream(wave);
    }

  public void disconnect()
    {
      stream = null;
    }

  public void initCheck()
	throws Error
    {
      if (stream==null)
	throw new Error("Uninitialised Data Source Error");
    }

  public String getContentType()
    {
      if (stream != null)
	return stream.getContentDescriptor().getContentType();
      return "audio.basic";
    }

  public void setLocator(MediaLocator l)
    {
      locator=l;
    }

  public MediaLocator getLocator()
    {
      return locator;
    }

  public void start()
    {
      initCheck();
    }
  
  public void stop()
    {
      initCheck();
    }

  public  PullSourceStream[] getStreams()
    {
      initCheck();
      return new PullSourceStream[] {stream};
    }

  public Time getDuration()
    {
      initCheck();
      return new Time((double)wave.samples.length/(double)wave.sample_rate);
    }

   public Object[] getControls()
    {
      return new Object[0];
    }

   public Object getControl(String controlType)
    {
      return null;
    }

}
