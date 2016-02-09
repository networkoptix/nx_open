
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
 //  Wrapper around a socket which is connected to Fringe.                 \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\

package cstr.est;

import java.lang.*;
import java.util.*;
import java.io.*;
import java.net.*;

public class ServerConnection
{
  private static String command_term = "//End//";
  private static String result_term = "//End//";

  private Socket s;
  private BufferedReader reader;
  private BufferedWriter writer;

  public ServerConnection(Socket sk)
    throws IOException
    {
      setSocket(sk);
    }

  public void setSocket(Socket sk)
    throws IOException
    {
      s=sk;

      try {
	if (s == null)
	  throw new IOException("no socket");
	
	  Reader r = new InputStreamReader(s.getInputStream());
	  reader = new BufferedReader(r);

	  Writer w = new OutputStreamWriter(s.getOutputStream());
	  writer =  new BufferedWriter(w);
      } catch (IOException e) {
	reader=null;
	writer=null;
	throw e;
      }
    }

  protected String readResult()
    throws IOException
    {
      while (true)
	{
	  StringBuffer res = new StringBuffer(100);
	  String status = null;
	  while (true)
	    {
	      String line = reader.readLine();
	      if (line.startsWith(result_term))
		{
		  status = line.substring(result_term.length());
		  break;
		}
	      res.append(line);
	    }
	  if (status.equals("OK"))
	    return res.toString();
	  else if (status.equals("ERROR"))
	    return "!"+res.toString();
	  else if (status.equals("VAL"))
	    System.err.println("Warning, intermediate value '"+res.toString()+"'");
	  else
	    throw new IOException("Uknknown value type "+status);
	}
    }

  public String sendCommand(String command)
    throws IOException
    {
      writer.write(command, 0, command.length());
      writer.newLine();
      writer.write(command_term, 0, command_term.length());
      writer.newLine();
      writer.flush();
	
      String result = readResult();
      
      return result;
    }

  public String sendCommand(StringBuffer command)
    throws IOException
    {
      return sendCommand(command.toString());
    }
}
