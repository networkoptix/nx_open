/*************************************************************************/
/*                Author :  Theo Veenker (Utrecht University)            */
/*                Date   :  September 1997                               */
/*-----------------------------------------------------------------------*/
/*  Optional 16bit linear support for audio on IRIS 4D workstations      */
/*                                                                       */
/*=======================================================================*/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include "EST_unix.h"
#include "EST_cutils.h"
#include "EST_Wave.h"
#include "EST_Option.h"
#include "audioP.h"
#include "EST_io_aux.h"

#if defined (SUPPORT_IRIX) || defined (SUPPORT_IRIX53)
#include <audio.h>
#include <unistd.h>

int irix_supported = TRUE;

int play_irix_wave(EST_Wave &inwave, EST_Option &al)
{
    int sample_rate;
    short *waveform;
    int num_samples;
    ALconfig config;
    ALport port;
    int r;
    (void)al;
    
    waveform = inwave.values().memory();
    num_samples = inwave.num_samples();
    sample_rate = inwave.sample_rate();
    
    config = ALnewconfig();
    ALsetsampfmt(config, AL_SAMPFMT_TWOSCOMP);
    ALsetwidth(config, AL_SAMPLE_16);
    ALsetchannels(config, AL_MONO);

    long pvbuf[2];
    pvbuf[0] = AL_OUTPUT_RATE;
    pvbuf[1] = sample_rate;
    ALsetparams(AL_DEFAULT_DEVICE, pvbuf, 2);

/*
    ALgetparams(AL_DEFAULT_DEVICE, pvbuf, 2);
    if (pvbuf[1] != sample_rate) 
    {
	cerr << "IRIX: sample rate " << sample_rate << 
	    " not supported; using " << pvbuf[1] << endl;
    }
*/
    
    port = ALopenport("speech-tools", "w", config);
    if (!port)
    {
	cerr << "IRIX: can't open audio port" << endl;
	ALfreeconfig(config);
	return -1;
    }

    r = ALwritesamps(port, waveform, num_samples);
    if (r != 0)
	cerr << "IRIX: failed to write to buffer" << endl;
    
    // Wait until all samples are played.
    // IRIX 5.3 doesn't have usleep
#ifdef SUPPORT_IRIX53
    while (ALgetfilled(port)) sginap(1);
#elseif
    while (ALgetfilled(port)) usleep(10000);
#endif
    
    ALcloseport(port);
    ALfreeconfig(config);
    
    return 1;
}

#else
int irix_supported = FALSE;

int play_irix_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "IRIX 16bit linear not supported" << endl;
    return -1;
}

#endif
