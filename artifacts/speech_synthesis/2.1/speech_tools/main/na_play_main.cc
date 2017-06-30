/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                      Copyright (c) 1995,1996                          */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*                  Authors: Paul Taylor, Alan Black                     */
/*                          (c) 1995-1999                                */
/*-----------------------------------------------------------------------*/
/*                     General play back program                         */
/*                                                                       */
/*=======================================================================*/

#include "EST.h"
#include "EST_audio.h"
#include "EST_cmd_line_options.h"

/** @name <command>na_play</command><emphasis> Audio Playback</emphasis>
    @id na_play_manual
  * @toc
 */

//@{


/**@name Synopsis
  */
//@{

//@synopsis

/**


na_play is a general playback program for playing sound files on a variety
of platforms and sound cards.

Currently, the following audio devices are supported:

<itemizedlist mark='@bullet'>
<listitem><para>sunaudio:</para><para>
8k ulaw direct to <filename>/dev/audio</filename> found on most Sun machines.  This
is also found under Linux and FreeBSD, and possibly others.  This
is the default if <function>netaudio</function> is not supported.
<listitem><para>netaudio:</para><para>
NCD's network transparent audio system (NAS).  This allows
use of audio devices across a network.  NAS has support for, Suns,
Linux, FreeBSD, HPs and probably other machines by now.
<listitem><para>sun16audio:</para><para>
This is only available on newer Sun workstations and has been
enabled at compile time.  This provides 
16bit linear PCM at various sample rates.
<listitem><para>linux16audio:</para><para>
This is only available on Linux workstations and has been enabled
at compile time.  This provides 
16bit linear PCM at various sample rates.
<listitem><para>freebsd16audio:</para><para>
This is only available on workstations running FreeBSD and has
been enabled at compile time.  This provides 
16bit linear PCM at various sample rates.
<listitem><para>mplayeraudio:</para><para>
This is only available under Windows NT 4.0 and Windows 95 and
has been enabled at compile time.  This provides 
16bit linear PCM at various sample rates.
<listitem><para>win32audio</para><para>
This is only available under Windows NT 4.0 and Windows 95 and
has been enabled at compile time.  This provides 
16bit linear PCM at various sample rates, playing the audio directly
rather than saving to a file as with mplayeraudio.
<listitem><para>irixaudio</para><para>
Audio support for SGI's IRIX 6.2.
<listitem><para>Audio_Command:</para><para>
Allows the specification of an arbitrary UNIX command to play
the waveform.  This won't normally be used with <function>na_play</function> as
you could just use the command directly but is necessary with some
systems using the speech tools.
</itemizedlist>
</para>

<para>
The default audio is netaudio if it is supported.  If not the platform
specific auido mode is the default (e.g. sun16audio, linux16audio,
freebsd16audio or mplayeraudio).  If none of these is supported,
sunaudio is the default. 
*/

//@}

/**@name OPTIONS
  */
//@{

//@options

//@}


int main (int argc, char *argv[])
{
    EST_Wave sig;
    EST_String in_file("-"), out_file("-");
    EST_StrList files;
    EST_Option al;
    EST_Litem *p;

    parse_command_line
	(argc,argv,
	 EST_String("[input file0] [input file1] ...\n")+
	 "Summary; play waveform files on audio device\n"+
	 "use \"-\" to make input and output files stdin/out\n"+
	 "-h               options help\n"+
	 options_wave_input()+
	 "-p <string>      audio device protocol. Ths supported types are\n"
	 "                 "+options_supported_audio()+"\n"
	 "-command <string> command to play wave when protocol\n"+
	 "                 is audio_command\n"
	 "-basic           HTML audio/basic format, if unheadered treat\n"
	 "                 as ulaw 8K\n\n"
	 "-r*              ESPS compatible way of selecting subrange of file.\n"
	 "                 The options -start, -end, -to and -from are \n"
	 "                 recommended\n\n"
	 "-quality <string>  either [ high | low ]. \"high\" will ensure \n"
	 "                 that proper resampling is used. \"low\" means play \n"
	 "                 as fast as possible, with a minimum of processing\n\n"
	 "-server <string> play sound on machine (when protocol is\n"
	 "                 server-based)\n"
	 "-audiodevice <string> use specified audiodevice if approrpriate\n"
	 "                 for protocol\n"
	 "-scale <float>    change the gain (volume) of the signal. 1.0 is default\n"
	 "-v               verbose. Print file names when playing\n"
	 "-wait            wait for a key to be pressed between each file\n",
	 files, al);

    // by default, parse_cl_ops adds a stdout file called "-". This is
    // irrelevant for na_play and needs to be removed.
    if (al.present("-server"))
	al.add_item("-display", al.val("-server"));

    for (p = files.head(); p; )
    {
	if (al.present("-v"))
	    cout << "playing " << files(p) << endl;

	if (read_wave(sig, files(p), al) != format_ok)
	    exit(-1);

	EST_Wave tmp, *toplay=&sig;

	if (al.present("-c"))
	  {
	    wave_extract_channel(tmp,sig,al.ival("-c"));
	    toplay=&tmp;
	  }

	/* 
	 * This is redundant as play_wave does this
	 *
	else  if (sig.num_channels() > 1)
	  {
	    wave_combine_channels(tmp, sig);
	    toplay=&tmp;
	  }
*/

	if (al.present("-scale"))
	  (*toplay).rescale(al.fval("-scale"));

	play_wave(*toplay, al);

	// pause for a keystroke between each file
	if (al.present("-wait") && p->next())
	{
	    if (getc(stdin) == 'a')
		continue;
	}
	p = p->next();
    }
    return 0;
}

void override_lib_ops(EST_Option &a_list, EST_Option &al)
{
    // Reorg -- can be deleted ?
    // general options
    a_list.override_val("sample_rate", al.val("-f", 0));
    
    // low pass filtering options.
    a_list.override_val("lpf_cutoff",al.val("-u", 0));
    a_list.override_val("lpf_order",al.val("-o", 0));
    
    if (al.val("-L", 0) == "true")
	a_list.override_val("do_low_pass", "true");
    if (al.val("-R", 0) == "true")
	a_list.override_val("do_low_pass", "false");
    a_list.override_val("color", al.val("-color", 0));    
    a_list.override_val("f0_file_type", al.val("-otype", 0));
    a_list.override_val("wave_file_type", al.val("-itype", 0));
}

