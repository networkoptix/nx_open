
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
 //  Enumerated type of utterance file formats.                            \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.est ;

import java.lang.*;
import java.util.*;
import java.awt.*;

import cstr.util.*;

public class UtteranceFileFormat extends Enum
{
  static EnumValues values = init("UtteranceFileFormat");
  
  public static UtteranceFileFormat DUMMY = new UtteranceFileFormat("dummy", "Dummy UtteranceFileFormat Value", false);
  public static UtteranceFileFormat UNKNOWN = new UtteranceFileFormat("unknown", "Unknown UtteranceFileFormat Value", false);

  public static UtteranceFileFormat EST_ASCII = new UtteranceFileFormat("est", "Est (ascii)");

  public static UtteranceFileFormat XLABEL = new UtteranceFileFormat("xlabel", "Xwaves Label File");

  public static UtteranceFileFormat GENXML = new UtteranceFileFormat("genxml", "Generic XML");

  public UtteranceFileFormat(String s, String ls)
    {
      super(s, ls, null);
    }

  public UtteranceFileFormat(String s, String ls, boolean real)
    {
      super(s, ls, real);
    }

  public EnumValues getValuesTable()
    {
      return values;
    }

  public static UtteranceFileFormat getValue(String s)
	    throws IllegalArgumentException
    {
      return (UtteranceFileFormat)getValue(s, values);
    }

  public static Enum [] getValues()
    {
      return (Enum [])getValues(values);
    }

  
}
