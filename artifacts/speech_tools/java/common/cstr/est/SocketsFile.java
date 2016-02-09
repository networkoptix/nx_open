
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
 //  Interface to the file in ~/.fringe/ which contains pointers to        \\
 //  running fringe process                                                \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\

package cstr.est ;

import java.lang.*;
import java.util.*;
import java.awt.*;
import java.io.*;
import java.net.*;

public class SocketsFile
{
  protected static Random random = new Random();
  protected String filename;

  public SocketsFile(String fn)
    {
      filename=fn;
    }

  public String setSocket(String type,
			  String name, 
			  ServerSocket s)
		    throws IOException
    {
      int port = s.getLocalPort();
      int cookie = random.nextInt();
      InetAddress address = s.getInetAddress();
      Properties props = new Properties();

      try {
	InputStream is = new FileInputStream(filename);

	props.load(is);
      } catch (IOException ex) {
      }

      props.put(name+".type", type);
      props.put(name+".host", address.getHostName());
      props.put(name+".address", address.getHostAddress());
      props.put(name+".cookie", Integer.toString(cookie));
      props.put(name+".port", Integer.toString(port));

      OutputStream os = new FileOutputStream(filename);

      props.save(os, "Fringe Sockets");
      
      return Integer.toString(cookie);
    }

  protected static void sendCookie(Socket s, String cookie)
    {
      try {
	Writer w = new OutputStreamWriter(s.getOutputStream());

	w.write("//");
	w.write(cookie);
	w.write("\n");
	w.flush();
      } catch (IOException ex) {
      }
    }

  public Socket getSocket(String myType, String name)
		    throws IOException
    {
      Socket s=null;
      Properties props = new Properties();

      InputStream is = new FileInputStream(filename);

      props.load(is);

      String type = props.getProperty(name+".type");
      String hostname = props.getProperty(name+".host");
      String address = props.getProperty(name+".address");
      String portStr =   props.getProperty(name+".port");
      String cookieStr =   props.getProperty(name+".cookie");

      if (!myType.equals(type))
	  return null;

      if (address != null && portStr != null)
	{
	  int port = Integer.parseInt(portStr);

	  s = new Socket(address, port);
	}

      if (s != null && cookieStr !=null && !cookieStr.equals("none"))
	sendCookie(s, cookieStr);

      return s;
    }
}
