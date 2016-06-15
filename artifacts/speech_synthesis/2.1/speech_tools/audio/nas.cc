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
/*                Author :  Paul Taylor & Alan Black                     */
/*                Date   :  April 1995 & 96                              */
/*-----------------------------------------------------------------------*/
/*  Playback function for NCD's NAS server (formerly called netaudio)    */
/*                                                                       */
/*=======================================================================*/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <sys/stat.h>
#include "EST_Wave.h"
#include "EST_Option.h"
#include "audioP.h"
#include "EST_io_aux.h"

#ifdef SUPPORT_NAS
#include <audio/audiolib.h>
#include <audio/soundlib.h>

#define	VOL(volume)		((1 << 16) * (volume) / 100)
#define au_serverrate 16000
static int nas_playing = 0;

int nas_supported = TRUE;

int endian_int = 1;
#define NAS_BIG_ENDIAN (((char *)&endian_int)[0] == 0)

static void na_sync_play_cb(AuServer *aud, AuEventHandlerRec *handler, 
                         AuEvent *ev, AuPointer data)
{
    int            *d = (int *) data;
    (void)aud;       // unused parameter
    (void)handler;   // unused parameter
    (void)ev;        // unused parameter
 
    nas_playing = 0;
    *d = 1;
    return;
}

int play_nas_wave(EST_Wave &inwave, EST_Option &al)
{
    AuServer *aud = NULL;
    /* legal sampling frequencies for Sun dbri device */
    /* But what about when we are using Linux of FreeBSD ? */
    int dev_sr[] = {8000, 9600, 11025, 16000, 18900, 22050, 32000, 
		    37800, 44100, 48000, -1};
    Sound in;
    char *auservername = NULL;
    int d = 0;
    int i;
    int ret;
    AuEvent ev;
    AuEventHandlerRec *er;
    short *waveform;
    int num_samps, samp_rate;

    if (al.present("-display"))
	auservername = wstrdup(al.val("-display"));

    aud = AuOpenServer(auservername, 0, NULL, 0, NULL, NULL);
    if (aud == NULL) 
    {
	cerr << "Can't access NAS server " << auservername << endl;
	return -1;
    }

    /* Check sample rate of server -- should really check the if it   */
    /* only supports individual sample rate of which this wave is not */
    /* one then we should resample.                                   */
    samp_rate = inwave.sample_rate();
    bool samp_rate_ok = FALSE;
    for (i=0; dev_sr[i] != -1; i++)
	if (samp_rate == dev_sr[i])
	    samp_rate_ok = TRUE;
    if (samp_rate_ok == FALSE)
    {
	if (samp_rate == 10000)
	    inwave.resample(9600);  // just sounds much better than 16000
	else 
	    inwave.resample(16000);
    }

    waveform = inwave.values().memory();
    num_samps = inwave.num_samples();
    samp_rate = inwave.sample_rate();

    if (num_samps > 0)
    {	
	if (inwave.sample_type() == "mulaw")
	    in = SoundCreate(SoundFileFormatNone, 
			     AuFormatULAW8, 
			     inwave.num_channels(),
			     samp_rate, num_samps, NULL);
	else
	    in = SoundCreate(SoundFileFormatNone, 
			     (NAS_BIG_ENDIAN ? 
			      AuFormatLinearSigned16MSB :
			      AuFormatLinearSigned16LSB),
			     inwave.num_channels(),
			     samp_rate, num_samps,NULL);
	
	er = AuSoundPlayFromData(aud, in, waveform, AuNone, VOL(100), 
				 na_sync_play_cb,
				 (AuPointer) &d,(AuFlowID *) NULL,
				 (int *) NULL, (int *) NULL, &ret);
	while (1)
	{
	    AuNextEvent(aud, AuTrue, &ev);
	    AuDispatchEvent(aud, &ev);
	    
	    if (d) break;
	}
    }
    
    AuCloseServer(aud);

    return 1;
}

int record_nas_wave(EST_Wave &wave, EST_Option &al)
{
    (void)wave;
    (void)al;

    cerr << "NAS: record not written yet\n";
    return -1;
}

#else
int nas_supported = FALSE;

int play_nas_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "NAS playback not supported" << endl;
    return -1;
}


int record_nas_wave(EST_Wave &wave, EST_Option &al)
{
    (void)wave;
    (void)al;
    cerr << "NAS record not supported" << endl;
    return -1;
}

#endif
