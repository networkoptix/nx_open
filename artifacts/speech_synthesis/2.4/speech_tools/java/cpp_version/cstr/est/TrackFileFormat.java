
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
 //  Enumerated type of track file formats.                                \\
 //                                                                        \\
 //\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\


package cstr.est ;

import java.lang.*;
import java.util.*;
import java.awt.*;

import cstr.util.*;

public class TrackFileFormat extends Enum
{
  static EnumValues values = init("TrackFileFormat");
  
  public static TrackFileFormat DUMMY = new TrackFileFormat("dummy", "Dummy TrackFileFormat Value Fnord", false);
  public static TrackFileFormat UNKNOWN = new TrackFileFormat("unknown", "Unknown TrackFileFormat Value Fnord", false);

  public static TrackFileFormat EST_BINARY = new TrackFileFormat("est_binary", "Est (binary)");
  public static TrackFileFormat EST_ASCII = new TrackFileFormat("est", "Est (ascii)");
  public static TrackFileFormat ESPS = new TrackFileFormat("esps", "Entropic FEA");
  public static TrackFileFormat HTK = new TrackFileFormat("htk", "HTK");
  public static TrackFileFormat HTK_FBANK = new TrackFileFormat("htk_fbank", "HTK (Filter Bank)");
  public static TrackFileFormat HTK_MFCC = new TrackFileFormat("htk_mfcc", "HTK (MFCC)");
  public static TrackFileFormat HTK_USER= new TrackFileFormat("htk_user", "HTK (user defined)");
  public static TrackFileFormat HTK_DISCRETE = new TrackFileFormat("htk_discrete", "HTK (discrete)");
  public static TrackFileFormat XMG = new TrackFileFormat("xmg", "Xmg");
  public static TrackFileFormat EMA = new TrackFileFormat("ema", "EMA");
  public static TrackFileFormat EMA_SWAPPED = new TrackFileFormat("ema_swapped", "EMA (swapped)");
  public static TrackFileFormat ASCII = new TrackFileFormat("ascii", "ASCII");

  public TrackFileFormat(String s, String ls)
    {
      super(s, ls, null);
    }

  public TrackFileFormat(String s, String ls, boolean real)
    {
      super(s, ls, real);
    }

  public EnumValues getValuesTable()
    {
      return values;
    }

  public static TrackFileFormat getValue(String s)
	    throws IllegalArgumentException
    {
      return (TrackFileFormat)getValue(s, values);
    }

  public static Enum [] getValues()
    {
      return (Enum [])getValues(values);
    }

  
}
