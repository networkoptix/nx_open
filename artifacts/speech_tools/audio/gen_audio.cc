/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                       Copyright (c) 1995,1996                         */
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
/*                      Author :  Paul Taylor                            */
/*                      Date   :  March 95                               */
/*-----------------------------------------------------------------------*/
/*                  Generalised playback function                        */
/*                                                                       */
/*=======================================================================*/

#include <cstdlib>
#include <iostream>
#include <cstring>
#include <cmath>
#include <fcntl.h>
#include "EST_system.h"
#include "EST_socket.h"
#include "EST_Option.h"
#include "EST_Wave.h"
#include "EST_io_aux.h"
#include "audioP.h"
#include "EST_audio.h"
#include "EST_wave_aux.h"

static int play_sunau_wave(EST_Wave &inwave, EST_Option &al);
static int play_socket_wave(EST_Wave &inwave, EST_Option &al);
static int play_aucomm_wave(EST_Wave &inwave, EST_Option &al);

static int record_sunau_wave(EST_Wave &wave, EST_Option &al);

int play_wave(EST_Wave &inwave, EST_Option &al)
{
    EST_String protocol;
    EST_Wave wtmp;
    EST_Wave *toplay;
    char *quality;
    char *sr;

    if ((sr = getenv("NA_PLAY_HOST")) != NULL)
	if  (!al.present("-display"))
	    al.add_item("-display", sr);
    
    if ((quality = getenv("NA_PLAY_QUALITY")) != NULL)
	if  (!al.present("-quality"))
	    al.add_item("-quality", quality);

    if (al.present("-p"))
	protocol = al.val("-p");
    else if ((sr=getenv("NA_PLAY_PROTOCOL")) != NULL)
	protocol = sr;
    else if (protocol == "")
    {
	if (nas_supported)
	    protocol = "netaudio";  // the default protocol
	else if (esd_supported)
	    protocol = "esdaudio";
	else if (sun16_supported)
	    protocol = "sun16audio";
	else if (freebsd16_supported)
	    protocol = "freebsd16audio";
	else if (linux16_supported)
	    protocol = "linux16audio";
	else if (irix_supported)
	    protocol = "irixaudio";
	else if (macosx_supported)
      protocol = "macosxaudio";
	else if (win32audio_supported)
	    protocol = "win32audio";
	else if (mplayer_supported)
	    protocol = "mplayeraudio";
	else
	    protocol = "sunaudio";
    }

  // OS X can handle multichannel audio, don't know about other systems.
  if (inwave.num_channels() > 1 && upcase(protocol) != "MACOSXAUDIO" )
    {
  	  wave_combine_channels(wtmp,inwave);
  	  toplay = &wtmp;
    }
  else
  	toplay = &inwave;

    if (upcase(protocol) == "NETAUDIO")
	return play_nas_wave(*toplay,al);
    else if (upcase(protocol) == "ESDAUDIO")
	return play_esd_wave(*toplay,al);
    else if (upcase(protocol) == "SUNAUDIO")
	return play_sunau_wave(*toplay,al);
    else if (upcase(protocol) == "SUN16AUDIO")
	return play_sun16_wave(*toplay,al);
    else if ((upcase(protocol) == "FREEBSD16AUDIO") ||
	     (upcase(protocol) == "LINUX16AUDIO"))
	return play_linux_wave(*toplay,al);
    else if (upcase(protocol) == "IRIXAUDIO")
	return play_irix_wave(*toplay,al);
    else if (upcase(protocol) == "MACOSXAUDIO")
	return play_macosx_wave(*toplay,al);
    else if (upcase(protocol) == "MPLAYERAUDIO")
	return play_mplayer_wave(*toplay,al);
    else if (upcase(protocol) == "WIN32AUDIO")
	return play_win32audio_wave(*toplay,al);
    else if (upcase(protocol) == "AUDIO_COMMAND")
	return play_aucomm_wave(*toplay,al);
    else if (upcase(protocol) == "SOCKET")
	return play_socket_wave(*toplay,al);
    else
    {
	cerr << "Unknown audio server protocol " << protocol << endl;
	return -1;
    }
}

static int play_socket_wave(EST_Wave &inwave, EST_Option &al)
{
    // Send inwave down the given fd (a socket)
    SOCKET_FD fd;
    EST_String otype;
    EST_String tmpfile = make_tmp_filename();

    if (al.present("socket_fd"))
	fd = al.ival("socket_fd");
    else
    {
	cerr << "Socket audio mode: no socket_fd specified" << endl;
	return -1;
    }

    if (al.present("socket_otype"))
	otype = al.val("socket_otype");  // file type to send to client
    else
	otype = "riff";
    
    inwave.save(tmpfile,otype);
    
    // Because the client may receive many different types of file
    // I send WV\n to it before the file itself
    send(fd,"WV\n",3,0);
    socket_send_file(fd,tmpfile);
    unlink(tmpfile);

    return 0;
}

static int play_aucomm_wave(EST_Wave &inwave, EST_Option &al)
{
    // Play wave by specified command 
    EST_String usrcommand, otype;
    char tmpfile[2048];
    char pref[2048];

    if (al.present("-command"))
	usrcommand = al.val("-command");
    else if (getenv("NA_PLAY_COMMAND") != NULL)
	usrcommand = getenv("NA_PLAY_COMMAND");
    else 
    {
	cerr << "Audio protocol set to COMMAND but no command specified\n";
	return -1;
    }

    sprintf(tmpfile,"/tmp/audiofile_%05ld",(long)getpid());

    if (al.present("-rate"))
	inwave.resample(al.ival("-rate"));
    if (al.present("-otype"))
	otype = al.val("-otype");
    else
	otype = "raw";

    if (inwave.save(tmpfile,otype) != write_ok)
    {
	cerr << "Audio writing file \"" << tmpfile << "\" in type \"" <<
	    otype << " failed " << endl;
	return -1;
    }

    sprintf(pref,"FILE=%s;SR=%d;",tmpfile,inwave.sample_rate());

    system((EST_String)pref+usrcommand.unquote('"'));

    unlink(tmpfile);  // so we don't fill up /tmp

    return 0;
}

static int play_sunau_wave(EST_Wave &inwave, EST_Option &al)
{
    // Play wave through /dev/audio using 8K ulaw encoding
    // works for Suns as well as Linux and FreeBSD machines
    int rcode;
    const char *audiodevice;

    inwave.resample(8000);

    if (al.present("-audiodevice"))
	audiodevice = al.val("-audiodevice");
    else
	audiodevice = "/dev/audio";

    // Should really do something cute about checking if /dev/audio
    // is not in use
    rcode = inwave.save(audiodevice,"ulaw");

    return rcode;

}

EST_String options_supported_audio(void)
{
    // returns list of supported audio types
    EST_String audios = "";

    audios += "sunaudio";  // we always support this in spite of the hardware

    audios += " audio_command";
    if (nas_supported)
	audios += " netaudio";
    else if (esd_supported)
	audios += " esdaudio";
    if (sun16_supported)
	audios += " sun16audio";
    if (freebsd16_supported)
	audios += " freebsd16audio";
    if (linux16_supported)
	audios += " linux16audio";
    if (irix_supported)
	audios += " irixaudio";
    if (mplayer_supported)
	audios += " mplayeraudio";
    if (macosx_supported)
	audios += "macosxaudio";
    if (win32audio_supported)
	audios += " win32audio";
    if (os2audio_supported)
	audios += " os2audio";

    return audios;
}

int record_wave(EST_Wave &wave, EST_Option &al)
{
    // Record wave from audio device 
    char *sr;
    EST_String protocol;

    // For archaic reasons, if you are using NAS use DISPLAY or
    // AUDIOSERVER 
    if ((sr = getenv("NA_PLAY_HOST")) != NULL)
	if  (!al.present("-display"))
	    al.add_item("-display", sr);

    if (al.present("-p"))
	protocol = al.val("-p");
    else if ((sr=getenv("NA_PLAY_PROTOCOL")) != NULL)
	protocol = sr;
    else if (protocol == "")
    {
	if (nas_supported)
	    protocol = "netaudio";  // the default protocol
	else if (esd_supported)
	    protocol = "esdaudio";  // the default protocol
	else if (sun16_supported)
	    protocol = "sun16audio";
	else if (freebsd16_supported)
	    protocol = "freebsd16audio";
	else if (linux16_supported)
	    protocol = "linux16audio";
	else if (irix_supported)
	    protocol = "irixaudio";
	else if (win32audio_supported)
	    protocol = "win32audio";
	else if (mplayer_supported)
	    protocol = "mplayeraudio";
	else
	    protocol = "sunaudio";
    }

    if (upcase(protocol) == "NETAUDIO")
	return record_nas_wave(wave,al);
    else if (upcase(protocol) == "ESDAUDIO")
        return record_esd_wave(wave,al);
    else if (upcase(protocol) == "SUN16AUDIO")
	return record_sun16_wave(wave,al);
    else if ((upcase(protocol) == "FREEBSD16AUDIO") ||
	     (upcase(protocol) == "LINUX16AUDIO"))
	return record_linux_wave(wave,al);
    else if (upcase(protocol) == "SUNAUDIO")
	return record_sunau_wave(wave,al);
    else
    {
	cerr << "NA_RECORD: \"" << protocol << 
	    "\" EST current has no record support" << endl;
	return -1;
    }
}

static int record_sunau_wave(EST_Wave &wave, EST_Option &al)
{
    int num_samples,i,r,n;
    int audio;
    unsigned char *ulawwave;
    short *waveform;
    const int AUDIOBUFFSIZE = 256;
    const char *audiodevice;

    if (al.present("-audiodevice"))
	audiodevice = al.val("-audiodevice");
    else
	audiodevice = "/dev/audio";

    if ((audio = open(audiodevice, O_RDONLY)) == -1)
    {
	cerr << "SUN16: can't open " << audiodevice << " for reading" << endl;
	return -1;
    }

    num_samples = (int)(8000*al.fval("-time"));
    ulawwave = walloc(unsigned char,num_samples);
    
    for (r=i=0; i < num_samples; i+= r)
    {
	if (num_samples > i+AUDIOBUFFSIZE)
	    n = AUDIOBUFFSIZE;
	else
	    n = num_samples-i;
	r = read(audio,&ulawwave[i], n);
	if (r <= 0)
	{
	    cerr << "sunaudio: failed to read from audio device" << endl;
	    close(audio);
	    wfree(ulawwave);
	    return -1;
	}
    }

    wave.resize(num_samples);
    wave.set_sample_rate(8000);
    waveform = wave.values().memory();

    ulaw_to_short(ulawwave,waveform,num_samples);
    wave.resample(al.ival("-sample_rate"));

    close(audio);
    wfree(ulawwave);
    return 0;
}

