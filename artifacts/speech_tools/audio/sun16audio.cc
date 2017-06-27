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
/*                Author :  Alan W Black                                 */
/*                Date   :  July 1997                                    */
/*-----------------------------------------------------------------------*/
/*  Optional Sun 16bit linear support for /dev/audio                     */
/*  This only works when compiled under Solaris or SunOS as it requires  */
/*  Sun's headers, and much more importantly Sun's /dev/audio.  This     */
/*  of course will access the *local* machine's /dev/audio definite not  */
/*  the "network is the computer" maxim but sometimes you might want     */
/*  this                                                                 */
/*                                                                       */
/*=======================================================================*/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "EST_cutils.h"
#include "EST_Wave.h"
#include "EST_Option.h"
#include "audioP.h"
#include "EST_io_aux.h"
#include "EST_unix.h"

#ifdef SUPPORT_SUN16
#include <sys/filio.h>
#if defined(__svr4__) || defined(SYSV) || defined(SVR4)
/* Solaris */
#include <sys/audioio.h>
#else
/* Sunos */
#include <sun/audioio.h>
#endif

static int sun16_check_device(int audio);
static int sun16_set_info(int audio, int sample_rate);

int sun16_supported = TRUE;

/* supported sampling frequencies for Sun dbri device */
static int dev_sr[] = {8000, 9600, 11025, 16000, 18900, 22050, 32000, 
		       37800, 44100, 48000, -1};

#define AUDIOBUFFSIZE 256

int play_sun16_wave(EST_Wave &inwave, EST_Option &al)
{
    int sample_rate;
    short *waveform;
    FILE *fdaudio;
    int num_samples;
    int audio;
    int i,r,n;
    char *audiodevice;

    sample_rate = inwave.sample_rate();
    int samp_rate_ok = FALSE;
    for (i=0; dev_sr[i] != -1; i++)
	if (sample_rate == dev_sr[i])
	    samp_rate_ok = TRUE;
    if (samp_rate_ok == FALSE)
    {
	if (sample_rate == 10000)
	    inwave.resample(9600);  // just sounds much better than 16000
	else 
	    inwave.resample(16000);
    }

    if (al.present("-audiodevice"))
	audiodevice = al.val("-audiodevice");
    else if ((audiodevice = getenv("AUDIODEV")) == NULL) 
	audiodevice = "/dev/audio";

    if ((fdaudio = fopen(audiodevice,"wb")) == NULL)
    {
	cerr << "SUN16: can't open " << audiodevice << endl;
	return -1;
    }
    // As I can't find open in an Sun CC include file I'll avoid it
    audio = fileno(fdaudio);

    waveform = inwave.values().memory();
    num_samples = inwave.num_samples();
    sample_rate = inwave.sample_rate();

    if (sun16_check_device(audio) == FALSE)
    {
	cerr << "SUN16: device doesn't support 16bit linear." << endl;
	fclose(fdaudio);
	return -1;
    }

    if (sun16_set_info(audio,sample_rate) == FALSE)
    {
	cerr << "SUN16: unable to set sample rate " <<
	    sample_rate << endl;
	fclose(fdaudio);
	return -1;
    }

    for (i=0; i < num_samples; i += r/2)
    {
	if (num_samples > i+AUDIOBUFFSIZE)
	    n = AUDIOBUFFSIZE;
	else
	    n = num_samples-i;
	r = write(audio,&waveform[i], n*2);
	if (r == 0)
	{
	    cerr << "SUN16: failed to write to buffer" <<
	    sample_rate << endl;
	    fclose(fdaudio);
	    return -1;
	}
	// needed to prevent foul ups under Java.
//	ioctl(audio, AUDIO_DRAIN, 0);
    }

    fclose(fdaudio);
    return 1;
}

static int sun16_check_device(int audio)
{
#ifdef __svr4__
/* Solaris */
    audio_device_t    type;
    
    ioctl(audio, AUDIO_DRAIN, 0);	       /* drain everything out */

    if ((ioctl(audio, AUDIO_GETDEV, &type) != -1) &&
	((streq("SUNW,CS4231",type.name)) ||   /* Newer Suns (ultras)      */
	 (streq("SUNW,dbri",type.name)) ||     /* Older Suns (SS10s SS20s) */
         (streq("SUNW,audiots",type.name)) ||  /* For stations more advanced than ultras */
	 (streq("SUNW,sb16",type.name))))      /* i386 machines            */
	return TRUE;
    else
	return FALSE;
#else
/* SunOS */
    int type;

    ioctl(audio, AUDIO_DRAIN, 0);	       /* drain everything out */

    if ((ioctl(audio, AUDIO_GETDEV, &type) != -1) &&
	((type == AUDIO_DEV_SPEAKERBOX) || (type == AUDIO_DEV_CODEC)))
	return TRUE;
    else
	return FALSE;
#endif

}
	
static int sun16_set_info(int audio, int sample_rate)
{
    audio_info_t    info;
    
    ioctl(audio, AUDIO_GETINFO, &info);

    info.play.sample_rate = sample_rate;
    info.play.encoding = AUDIO_ENCODING_LINEAR;
    info.play.precision = 16;
    info.play.channels = 1;

    if (ioctl(audio, AUDIO_SETINFO, &info) == -1)
	return FALSE;
    else
	return TRUE;
}

static int sun16_setrecord_info(int audio, int sample_rate)
{
    /* As the device is always recording, changing sample rate/encoding */
    /* can mess up the stream, so stop recording, flush the buffer and  */
    /* then set the formats and restart recording                       */
    audio_info_t    info;
    int read_size,i,r;
    unsigned char buff[64];
    
    ioctl(audio, AUDIO_GETINFO, &info);

    info.record.pause = 1;

    ioctl(audio, AUDIO_SETINFO, &info);
    
    /* Read any existing recorded stuff in the buffer */
    ioctl(audio, FIONREAD, &read_size);
    for (r=i=0; (i < read_size); i += r)
	r = read(audio,buff,64);

    /* Now set up the recording format */
    ioctl(audio, AUDIO_GETINFO, &info);
    info.record.sample_rate = sample_rate;
    info.record.encoding = AUDIO_ENCODING_LINEAR;
    info.record.precision = 16;
    info.record.channels = 1;
    info.record.pause = 0;
    info.record.samples = 0;
    info.record.error = 0;

    if (ioctl(audio, AUDIO_SETINFO, &info) == -1)
	return FALSE;
    else
	return TRUE;
}

int record_sun16_wave(EST_Wave &wave, EST_Option &al)
{
    int desired_sample_rate = 16000;
    int actual_sample_rate;
    short *waveform;
    int audio=-1;
    int num_samples;
    int i,r,n;

    desired_sample_rate = al.ival("-sample_rate");
    actual_sample_rate = -1;
    for (i=0; dev_sr[i] != -1; i++)
	if (desired_sample_rate == dev_sr[i])
	    actual_sample_rate = desired_sample_rate;
    if (actual_sample_rate == -1)
	actual_sample_rate = 16000;
    
    if ((audio = open("/dev/audio",O_RDONLY)) == -1)
    {
	cerr << "SUN16: can't open /dev/audio for reading" << endl;
	return -1;
    }

    if (sun16_check_device(audio) == FALSE)
    {
	cerr << "SUN16: device doesn't support 16bit linear." << endl;
	close(audio);
	return -1;
    }

    if (sun16_setrecord_info(audio,actual_sample_rate) == FALSE)
    {
	cerr << "SUN16: unable to set sample rate " <<
	    actual_sample_rate << endl;
	close(audio);
	return -1;
    }

    wave.resize((int)(actual_sample_rate*al.fval("-time")));
    wave.set_sample_rate(actual_sample_rate);
    num_samples = wave.num_samples();
    waveform = wave.values().memory();

    int read_size;

    for (r=i=0; i < num_samples; i+= r)
    {
	if (num_samples > i+AUDIOBUFFSIZE)
	    n = AUDIOBUFFSIZE;
	else
	    n = num_samples-i;
	ioctl(audio, FIONREAD, &read_size);
	if (read_size == 0)
	{
	    r = 0;      
	    continue;     // nothing to read yet
	}
	if (n > read_size/2)
	    n = read_size/2;
	r = read(audio,&waveform[i], n*2);
	r /= 2;
	if (r <= 0)
	{
	    cerr << "SUN16: failed to read from audio device" << endl;
	    close(audio);
	    return -1;
	}

    }

    close(audio);
    if (actual_sample_rate != desired_sample_rate)
	wave.resample(desired_sample_rate);
    return 0;
}

#else
int sun16_supported = FALSE;

int play_sun16_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "Sun 16bit linear not supported" << endl;
    return -1;
}

int record_sun16_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "Sun 16bit linear not supported" << endl;
    return -1;
}

#endif
