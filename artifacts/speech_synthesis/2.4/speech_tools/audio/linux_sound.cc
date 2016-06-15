/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                      Copyright (c) 1997,1998                          */
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
/*  Optional support for /dev/dsp under FreeBSD and Linux                */
/*  These use the same underlying sound drivers (voxware).  This uses    */
/*  16bit linear if the device supports it otherwise it uses 8bit.  The  */
/*  8bit driver is still better than falling back to the "sunaudio" ulaw */
/*  8K as this driver can cope with various sample rates (and saves on   */
/*  resampling).                                                         */
/*                                                                       */
/*  Combined FreeBSD and Voxware code Feb 98                             */
/*                                                                       */
/*  This may work on NetBSD and OpenBSD but I haven't tried it           */
/*                                                                       */
/*=======================================================================*/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <sys/stat.h>
#include "EST_cutils.h"
#include "EST_walloc.h"
#include "EST_Wave.h"
#include "EST_wave_aux.h"
#include "EST_Option.h"
#include "audioP.h"
#include "EST_io_aux.h"
#include "EST_error.h"

#ifdef SUPPORT_FREEBSD16
#include <sys/soundcard.h>
#include <fcntl.h>
int freebsd16_supported = TRUE;
int linux16_supported = FALSE;
static char *aud_sys_name = "FreeBSD";
#endif /*SUPPORT_FREEBSD16 */

#ifdef SUPPORT_VOXWARE

#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
int linux16_supported = TRUE;
int freebsd16_supported = FALSE;
static const char *aud_sys_name = "Linux";
static int stereo_only = 0;

// Code to block signals while sound is playing.
// Needed inside Java on (at least some) linux systems
// as scheduling interrupts seem to break the writes. 

#if defined(SUPPORT_LINUX16) || defined(SUPPORT_FREEBSD16)

#include <csignal>
#include <pthread.h>

#define THREAD_DECS() \
    sigset_t  oldmask \

#define THREAD_PROTECT() do { \
    sigset_t  newmask; \
    \
    sigfillset(&newmask); \
    \
    pthread_sigmask(SIG_BLOCK, &newmask, &oldmask); \
    } while(0)

#define THREAD_UNPROTECT() do { \
     pthread_sigmask(SIG_SETMASK, &oldmask, NULL); \
     } while (0)

#else
#define  THREAD_DECS() //empty
#define  THREAD_PROTECT() //empty
#define  THREAD_UNPROTECT() //empty
#endif /* LINUX_16/FREEBSD16 */

static int sb_set_sample_rate(int sbdevice, int samp_rate)
{
    int fmt;
    int sfmts;
    int stereo=0;
    int sstereo;
    int channels=1;

    ioctl(sbdevice,SNDCTL_DSP_RESET,0);
    ioctl(sbdevice,SNDCTL_DSP_SPEED,&samp_rate);
    sstereo = stereo;
    ioctl(sbdevice,SNDCTL_DSP_STEREO,&sstereo);
    /* Some devices don't do mono even when you ask them nicely */
    if (sstereo != stereo)
        stereo_only = 1;
    ioctl(sbdevice,SNDCTL_DSP_CHANNELS,&channels);
    ioctl(sbdevice,SNDCTL_DSP_GETFMTS,&sfmts);

    if (sfmts == AFMT_U8)
	fmt = AFMT_U8;         // its really an 8 bit only device
    else if (EST_LITTLE_ENDIAN)
	fmt = AFMT_S16_LE;  
    else
	fmt = AFMT_S16_BE;  
    
    ioctl(sbdevice,SNDCTL_DSP_SETFMT,&fmt);
    
    return fmt;
}

#define AUDIOBUFFSIZE 256
// #define AUDIOBUFFSIZE 20480

int play_linux_wave(EST_Wave &inwave, EST_Option &al)
{
    int sample_rate;
    short *waveform;
    short *waveform2 = 0;
    int num_samples;
    int audio,actual_fmt;
    int i,r,n;
    const char *audiodevice;

    if (al.present("-audiodevice"))
	audiodevice = al.val("-audiodevice");
    else
	audiodevice = "/dev/dsp";

    if ((audio = open(audiodevice,O_WRONLY)) == -1)
    {
	cerr << aud_sys_name << ": can't open " << audiodevice << endl;
	return -1;
    }
 
    // int tmp=open("/tmp/vox_play_wave",O_WRONLY|O_CREAT);
    
    waveform = inwave.values().memory();
    num_samples = inwave.num_samples();
    sample_rate = inwave.sample_rate();

    actual_fmt = sb_set_sample_rate(audio,sample_rate);

    if (stereo_only)
    {
	waveform2 = walloc(short,num_samples*2);
	for (i=0; i<num_samples; i++)
	{
	    waveform2[i*2] = inwave.a(i);
	    waveform2[(i*2)+1] = inwave.a(i);
	}
	waveform = waveform2;
	num_samples *= 2;
    }

    THREAD_DECS();
    THREAD_PROTECT();
    
    if (sb_set_sample_rate(audio,sample_rate) == AFMT_U8)
    {
	// Its actually 8bit unsigned so convert the buffer;
	unsigned char *uchars = walloc(unsigned char,num_samples);
	for (i=0; i < num_samples; i++)
	    uchars[i] = waveform[i]/256+128;
	for (i=0; i < num_samples; i += r)
	{
	    if (num_samples > i+AUDIOBUFFSIZE)
		n = AUDIOBUFFSIZE;
	    else
		n = num_samples-i;
	    // r = write(tmp,&uchars[i], n);
	    r = write(audio,&uchars[i], n);
	    if (r == 0)
	    {
		THREAD_UNPROTECT();
		cerr << aud_sys_name << ": failed to write to buffer" <<
		    sample_rate << endl;
		close(audio);
		return -1;
	    }
	}
	wfree(uchars);
    }
    else if ((actual_fmt == AFMT_S16_LE) || 
	     (actual_fmt == AFMT_S16_BE))
    {
      int blksize, nbuf, c;
      short *buf;

      ioctl (audio, SNDCTL_DSP_GETBLKSIZE, &blksize);
      nbuf=blksize;
      buf=new short[nbuf];

      for (i=0; i < num_samples; i += r/2)
	{
	  if (num_samples > i+nbuf)
	    n = nbuf;
	  else
	    n = num_samples-i;

	  for(c=0; c<n;c++)
	    buf[c]=waveform[c+i];

	  for(; c<nbuf;c++)
	    buf[c]=waveform[n-1];

	  // r = write(tmp,&waveform[i], n*2);
	  // r = write(audio,&waveform[i], n*2);
	  r=write(audio, buf, nbuf*2);
	  if (r <= 0)
	    {
	      THREAD_UNPROTECT();
	      EST_warning("%s: failed to write to buffer (sr=%d)",aud_sys_name, sample_rate );
		close(audio);
		return -1;
	    }
	    // ioctl(audio, SNDCTL_DSP_SYNC, 0);
	  // fprintf(stderr,"[%d]", r);
	}
      delete [] buf;
    }
    else
    {
      THREAD_UNPROTECT();
      cerr << aud_sys_name << ": unable to set sample rate " <<
	sample_rate << endl;
      close(audio);
      return -1;
    }
    
    // ioctl(audio, SNDCTL_DSP_SYNC, 0);
//    fprintf(stderr, "End Play\n");

    // close(tmp);
    close(audio);
    if (waveform2)
	wfree(waveform2);
    THREAD_UNPROTECT();
    return 1;
}

int record_linux_wave(EST_Wave &inwave, EST_Option &al)
{
    int sample_rate=16000;  // egcs needs the initialized for some reason
    short *waveform;
    short *waveform2=0;
    int num_samples;
    int audio=-1,actual_fmt;
    int i,r,n;
    const char *audiodevice;

    if (al.present("-audiodevice"))
	audiodevice = al.val("-audiodevice");
    else
	audiodevice = "/dev/dsp";

    sample_rate = al.ival("-sample_rate");
    
    if ((audio = open(audiodevice,O_RDONLY)) == -1)
    {
	cerr << aud_sys_name << ": can't open " << audiodevice
	     << "for reading" << endl;
	return -1;
    }
    
    actual_fmt = sb_set_sample_rate(audio,sample_rate);

    if ((actual_fmt == AFMT_S16_LE) || 
	(actual_fmt == AFMT_S16_BE))
    {
	// We assume that the device returns audio in native byte order
	// by default
	inwave.resize((int)(sample_rate*al.fval("-time")));
	inwave.set_sample_rate(sample_rate);
	num_samples = inwave.num_samples();
	waveform = inwave.values().memory();

	if (stereo_only)
	{
	    waveform2 = walloc(short,num_samples*2);
	    num_samples *= 2;
	}
	else
	    waveform2 = waveform;

	for (i=0; i < num_samples; i+= r)
	{
	    if (num_samples > i+AUDIOBUFFSIZE)
		n = AUDIOBUFFSIZE;
	    else
		n = num_samples-i;
	    r = read(audio,&waveform2[i], n*2);
	    r /= 2;
	    if (r <= 0)
	    {
		cerr << aud_sys_name << ": failed to read from audio device"
		    << endl;
		close(audio);
		return -1;
	    }
	}

    }
    else if (actual_fmt == AFMT_U8)
    {
	inwave.resize((int)(sample_rate*al.fval("-time")));
	inwave.set_sample_rate(sample_rate);
	num_samples = inwave.num_samples();
	waveform = inwave.values().memory();
	unsigned char *u8wave = walloc(unsigned char,num_samples);

	for (i=0; i < num_samples; i+= r)
	{
	    if (num_samples > i+AUDIOBUFFSIZE)
		n = AUDIOBUFFSIZE;
	    else
		n = num_samples-i;
	    r = read(audio,&u8wave[i],n);
	    if (r <= 0)
	    {
		cerr << aud_sys_name << ": failed to read from audio device"
		    << endl;
		close(audio);
		wfree(u8wave);
		return -1;
	    }
	    
	}
	uchar_to_short(u8wave,waveform,num_samples);
	wfree(u8wave);
    }
    else
    {
	cerr << aud_sys_name << ": unknown audio format from device: " << 
	    actual_fmt << endl;
	close(audio);
	return -1;
    }

    if (stereo_only)
    {
	for (i=0; i<num_samples; i+=2)
	    waveform[i/2] = waveform2[i];
	wfree(waveform2);
    }

    close(audio);
    return 0;
}

#else 

/*-----------------------------------------------------------------------*/
/*  Support for alsa, the voxware stuff just doesn't work on most        */
/*  machines now.  This code is a modification of the vanilla voxware    */
/*  support                                                              */
/*                                                                       */
/*  Based on the alsa support in Flite provided by Lukas Loehrer         */
/*                                                                       */
/*=======================================================================*/

#ifdef SUPPORT_ALSALINUX
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
static const char *aud_sys_name = "ALSALINUX";

// Code to block signals while sound is playing.
// Needed inside Java on (at least some) linux systems
// as scheduling interrupts seem to break the writes. 

int linux16_supported = TRUE;
int freebsd16_supported = FALSE;

#ifdef THREAD_SAFETY
#include <csignal>
#include <pthread.h>

#define THREAD_DECS() \
    sigset_t  oldmask \

#define THREAD_PROTECT() do { \
    sigset_t  newmask; \
    \
    sigfillset(&newmask); \
    \
    pthread_sigmask(SIG_BLOCK, &newmask, &oldmask); \
    } while(0)

#define THREAD_UNPROTECT() do { \
     pthread_sigmask(SIG_SETMASK, &oldmask, NULL); \
     } while (0)

#else
#define  THREAD_DECS() //empty
#define  THREAD_PROTECT() //empty
#define  THREAD_UNPROTECT() //empty
#endif /* THREAD_SAFETY */

static const char *pcm_dev_name ="default";

typedef enum {
    CST_AUDIO_LINEAR16 = 0,
    CST_AUDIO_LINEAR8,
    CST_AUDIO_MULAW
} cst_audiofmt;

typedef struct cst_audiodev_struct {
    int sps, real_sps;
    int channels, real_channels;
    cst_audiofmt fmt, real_fmt;
    int byteswap;
    /*    cst_rateconv *rateconv; */
    void *platform_data;
} cst_audiodev;

static int audio_bps(cst_audiofmt fmt)
{
    switch (fmt)
    {
    case CST_AUDIO_LINEAR16:
	return 2;
    case CST_AUDIO_LINEAR8:
    case CST_AUDIO_MULAW:
	return 1;
    }
    return 0;
}

static inline void print_pcm_state(snd_pcm_t *handle, char *msg)
{
  fprintf(stderr, "PCM state at %s = %s\n", msg,
		  snd_pcm_state_name(snd_pcm_state(handle)));
}

cst_audiodev *audio_open_alsa(int sps, int channels, cst_audiofmt fmt)
{
  cst_audiodev *ad;
  unsigned 	int real_rate;
  int err;

  /* alsa specific stuff */
  snd_pcm_t *pcm_handle;          
  snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
  snd_pcm_hw_params_t *hwparams;
  snd_pcm_format_t format;
  snd_pcm_access_t access = SND_PCM_ACCESS_RW_INTERLEAVED;

  /* Allocate the snd_pcm_hw_params_t structure on the stack. */
  snd_pcm_hw_params_alloca(&hwparams);

  /* Open pcm device */
  err = snd_pcm_open(&pcm_handle, pcm_dev_name, stream, 0);
  if (err < 0) 
  {
      EST_warning("audio_open_alsa: failed to open audio device %s. %s\n",
                  pcm_dev_name, snd_strerror(err));
      return NULL;
  }

  /* Init hwparams with full configuration space */
  err = snd_pcm_hw_params_any(pcm_handle, hwparams);
  if (err < 0) 
  {
	snd_pcm_close(pcm_handle);
	EST_warning("audio_open_alsa: failed to get hardware parameters from audio device. %s\n", snd_strerror(err));
	return NULL;
  }

  /* Set access mode */
  err = snd_pcm_hw_params_set_access(pcm_handle, hwparams, access);
  if (err < 0) 
  {
	snd_pcm_close(pcm_handle);
	EST_warning("audio_open_alsa: failed to set access mode. %s.\n", snd_strerror(err));
	return NULL;
  }

  /* Determine matching alsa sample format */
  /* This could be implemented in a more */
  /* flexible way (byte order conversion). */
  switch (fmt)
  {
  case CST_AUDIO_LINEAR16:
	if (EST_LITTLE_ENDIAN)
	  format = SND_PCM_FORMAT_S16_LE;
	else
	  format = SND_PCM_FORMAT_S16_BE;
	break;
  case CST_AUDIO_LINEAR8:
	format = SND_PCM_FORMAT_U8;
	break;
  case CST_AUDIO_MULAW:
	format = SND_PCM_FORMAT_MU_LAW;
	break;
  default:
	snd_pcm_close(pcm_handle);
	EST_warning("audio_open_alsa: failed to find suitable format.\n");
	return NULL;
	break;
  }

  /* Set samble format */
  err = snd_pcm_hw_params_set_format(pcm_handle, hwparams, format);
  if (err <0) 
  {
	snd_pcm_close(pcm_handle);
	EST_warning("audio_open_alsa: failed to set format. %s.\n", snd_strerror(err));
	return NULL;
  }

  /* Set sample rate near the disired rate */
  real_rate = sps;
  err = snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &real_rate, 0);
  if (err < 0)   
  {
	snd_pcm_close(pcm_handle);
	EST_warning("audio_open_alsa: failed to set sample rate near %d. %s.\n", sps, snd_strerror(err));
	return NULL;
  }

  /* Set number of channels */
  assert(channels >0);
  err = snd_pcm_hw_params_set_channels(pcm_handle, hwparams, channels);
  if (err < 0) 
  {
	snd_pcm_close(pcm_handle);
	EST_warning("audio_open_alsa: failed to set number of channels to %d. %s.\n", channels, snd_strerror(err));
	return NULL;
  }

  /* Commit hardware parameters */
  err = snd_pcm_hw_params(pcm_handle, hwparams);
  if (err < 0) 
  {
	snd_pcm_close(pcm_handle);
	EST_warning("audio_open_alsa: failed to set hw parameters. %s.\n", snd_strerror(err));
	return NULL;
  }

  /* Make sure the device is ready to accept data */
  assert(snd_pcm_state(pcm_handle) == SND_PCM_STATE_PREPARED);

  /* Write hardware parameters to flite audio device data structure */
  ad = walloc(cst_audiodev, 1);
  assert(ad != NULL);
  ad->real_sps = ad->sps = sps;
  ad->real_channels = ad->channels = channels;
  ad->real_fmt = ad->fmt = fmt;
  ad->platform_data = (void *) pcm_handle;

  return ad;
}

int audio_close_alsa(cst_audiodev *ad)
{
  int result;
  snd_pcm_t *pcm_handle;

  if (ad == NULL)
      return 0;

  pcm_handle = (snd_pcm_t *) ad->platform_data;

  snd_pcm_drain(pcm_handle); /* wait for current stuff in buffer to finish */

  result = snd_pcm_close(pcm_handle);
  if (result < 0)
  {
	EST_warning("audio_close_alsa: Error: %s.\n", snd_strerror(result));
  }
  wfree(ad);
  return result;
}

/* Returns zero if recovery was successful. */
static int recover_from_error(snd_pcm_t *pcm_handle, ssize_t res)
{
  if (res == -EPIPE) /* xrun */
  {
	res = snd_pcm_prepare(pcm_handle);
	if (res < 0) 
	{
	  /* Failed to recover from xrun */
	  EST_warning("recover_from_write_error: failed to recover from xrun. %s\n.", snd_strerror(res));
	  return res;
	}
  } 
  else if (res == -ESTRPIPE) /* Suspend */
  {
	while ((res = snd_pcm_resume(pcm_handle)) == -EAGAIN) 
	{
	  snd_pcm_wait(pcm_handle, 1000);
	}
	if (res < 0) 
	{
	  res = snd_pcm_prepare(pcm_handle);
	  if (res <0) 
	  {
		/* Resume failed */
		EST_warning("audio_recover_from_write_error: failed to resume after suspend. %s\n.", snd_strerror(res));
		return res;
	  }
	}
  } 
  else if (res < 0) 
  {
	/* Unknown failure */
	EST_warning("audio_recover_from_write_error: %s.\n", snd_strerror(res));
	return res;
  }
  return 0;
}

int audio_write_alsa(cst_audiodev *ad, void *samples, int num_bytes)
{
  size_t frame_size;
  ssize_t num_frames, res;
  snd_pcm_t *pcm_handle;
  char *buf = (char *) samples;

  /* Determine frame size in bytes */
  frame_size  = audio_bps(ad->real_fmt) * ad->real_channels;
  /* Require that only complete frames are handed in */
  assert((num_bytes % frame_size) == 0);
  num_frames = num_bytes / frame_size;
  pcm_handle = (snd_pcm_t *) ad->platform_data;

  while (num_frames > 0) 
  {
	res = snd_pcm_writei(pcm_handle, buf, num_frames);
	if (res != num_frames) 
	{
	  if (res == -EAGAIN || (res > 0 && res < num_frames)) 
	  {
		snd_pcm_wait(pcm_handle, 100);
	  }
	  else if (recover_from_error(pcm_handle, res) < 0) 
	  {
		return -1;
	  }
	}

	if (res >0) 
	{
	  num_frames -= res;
	  buf += res * frame_size;
	}
  }
  return num_bytes;
}

int audio_flush_alsa(cst_audiodev *ad)
{
  int result;
  result = snd_pcm_drain((snd_pcm_t *) ad->platform_data);
  if (result < 0)
  {
	EST_warning("audio_flush_alsa: Error: %s.\n", snd_strerror(result));
  }
	/* Prepare device for more data */
  result = snd_pcm_prepare((snd_pcm_t *) ad->platform_data);
if (result < 0)
  {
	EST_warning("audio_flush_alsa: Error: %s.\n", snd_strerror(result));
  }
  return result;
}

int audio_drain_alsa(cst_audiodev *ad)
{
  int result;
  result = snd_pcm_drop((snd_pcm_t *) ad->platform_data);
  if (result < 0)
  {
	EST_warning("audio_drain_alsa: Error: %s.\n", snd_strerror(result));
  }
/* Prepare device for more data */
  result = snd_pcm_prepare((snd_pcm_t *) ad->platform_data);
if (result < 0)
  {
	EST_warning("audio_drain_alsa: Error: %s.\n", snd_strerror(result));
  }
  return result;
}

#define AUDIOBUFFSIZE 256
// #define AUDIOBUFFSIZE 20480

int play_linux_wave(EST_Wave &inwave, EST_Option &al)
{
    int sample_rate;
    short *waveform;
    int num_samples;
    const char *audiodevice;
    cst_audiodev *ad;

    if (al.present("-audiodevice"))
	audiodevice = al.val("-audiodevice");
    else
	audiodevice = "/dev/dsp";

    waveform = inwave.values().memory();
    num_samples = inwave.num_samples();
    sample_rate = inwave.sample_rate();

    ad = audio_open_alsa(sample_rate,1,CST_AUDIO_LINEAR16);
 
    THREAD_DECS();
    THREAD_PROTECT();

    audio_write_alsa(ad,waveform,num_samples*sizeof(short));
    
    audio_close_alsa(ad);

    THREAD_UNPROTECT();
    return 1;
}

int record_linux_wave(EST_Wave &inwave, EST_Option &al)
{
#if 0
    int sample_rate=16000;  // egcs needs the initialized for some reason
    short *waveform;
    short *waveform2=0;
    int num_samples;
    int audio=-1,actual_fmt;
    int i,r,n;
    char *audiodevice;

    if (al.present("-audiodevice"))
	audiodevice = al.val("-audiodevice");
    else
	audiodevice = "/dev/dsp";

    sample_rate = al.ival("-sample_rate");
    
    if ((audio = open(audiodevice,O_RDONLY)) == -1)
    {
	cerr << aud_sys_name << ": can't open " << audiodevice
	     << "for reading" << endl;
	return -1;
    }
    
    actual_fmt = sb_set_sample_rate(audio,sample_rate);

    if ((actual_fmt == AFMT_S16_LE) || 
	(actual_fmt == AFMT_S16_BE))
    {
	// We assume that the device returns audio in native byte order
	// by default
	inwave.resize((int)(sample_rate*al.fval("-time")));
	inwave.set_sample_rate(sample_rate);
	num_samples = inwave.num_samples();
	waveform = inwave.values().memory();

	if (stereo_only)
	{
	    waveform2 = walloc(short,num_samples*2);
	    num_samples *= 2;
	}
	else
	    waveform2 = waveform;

	for (i=0; i < num_samples; i+= r)
	{
	    if (num_samples > i+AUDIOBUFFSIZE)
		n = AUDIOBUFFSIZE;
	    else
		n = num_samples-i;
	    r = read(audio,&waveform2[i], n*2);
	    r /= 2;
	    if (r <= 0)
	    {
		cerr << aud_sys_name << ": failed to read from audio device"
		    << endl;
		close(audio);
		return -1;
	    }
	}

    }
    else if (actual_fmt == AFMT_U8)
    {
	inwave.resize((int)(sample_rate*al.fval("-time")));
	inwave.set_sample_rate(sample_rate);
	num_samples = inwave.num_samples();
	waveform = inwave.values().memory();
	unsigned char *u8wave = walloc(unsigned char,num_samples);

	for (i=0; i < num_samples; i+= r)
	{
	    if (num_samples > i+AUDIOBUFFSIZE)
		n = AUDIOBUFFSIZE;
	    else
		n = num_samples-i;
	    r = read(audio,&u8wave[i],n);
	    if (r <= 0)
	    {
		cerr << aud_sys_name << ": failed to read from audio device"
		    << endl;
		close(audio);
		wfree(u8wave);
		return -1;
	    }
	    
	}
	uchar_to_short(u8wave,waveform,num_samples);
	wfree(u8wave);
    }
    else
    {
	cerr << aud_sys_name << ": unknown audio format from device: " << 
	    actual_fmt << endl;
	close(audio);
	return -1;
    }

    if (stereo_only)
    {
	for (i=0; i<num_samples; i+=2)
	    waveform[i/2] = waveform2[i];
	wfree(waveform2);
    }

    close(audio);
#endif /* 0 */ 
    return 0;
}

#else

#ifdef SUPPORT_PULSEAUDIO
#include <pulse/simple.h>

int freebsd16_supported = FALSE;
int linux16_supported = TRUE;

static const char *aud_sys_name = "PULSEAUDIO";

#define AUDIOBUFFSIZE 256
// #define AUDIOBUFFSIZE 20480

int play_linux_wave(EST_Wave &inwave, EST_Option &al)
{
    pa_sample_spec *ss;
    pa_simple *s;
    short *waveform;
    int num_samples;
    int err=0, i, r;

    ss = walloc(pa_sample_spec,1);
    ss->rate = inwave.sample_rate();
    ss->channels = inwave.num_channels();

    if (EST_BIG_ENDIAN)
        ss->format = PA_SAMPLE_S16BE;
    else
        ss->format = PA_SAMPLE_S16LE;
    
    s = pa_simple_new(
                    NULL,      /* use default server */
                    "festival",
                    PA_STREAM_PLAYBACK,
                    NULL,      /* use default device */
                    "Speech",
                    ss,
                    NULL,      /* default channel map */
                    NULL,      /* default buffering attributes */
                    &err);
    if (err < 0)
        return NULL;

    waveform = inwave.values().memory();
    num_samples = inwave.num_samples();

    for (i=0; i < num_samples; i += AUDIOBUFFSIZE/2)
    {
        if (i + AUDIOBUFFSIZE/2 < num_samples)
            pa_simple_write(s,&waveform[i],(size_t)AUDIOBUFFSIZE,&err);
        else
            pa_simple_write(s,&waveform[i],(size_t)(num_samples-i)*2,&err);
    }

    pa_simple_drain(s,&err);
    pa_simple_free(s);
    wfree(ss);

    return 1;
}

int record_linux_wave(EST_Wave &inwave, EST_Option &al)
{
    return -1;
}

#else /* not supported */

int freebsd16_supported = FALSE;
int linux16_supported = FALSE;

int play_linux_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "MacOS X audio support not compiled." << endl;
    return -1;
}
int record_linux_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "MacOS X audio support not compiled." << endl;
    return -1;
}

#endif /* ALSA */
#endif /* PULSEAUDIO */
#endif /* VOXWARE */

