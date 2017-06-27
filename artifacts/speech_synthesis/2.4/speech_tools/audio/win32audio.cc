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
/*                Author :  Richard Caley                                */
/*                Date   :  August 1997                                  */
/*-----------------------------------------------------------------------*/
/*  Optional 16bit linear support for Windows NT/95                      */
/*                                                                       */
/*=======================================================================*/

#include <cstdio>
#include "EST_cutils.h"
#include "EST_Wave.h"
#include "EST_Option.h"
#include "EST_io_aux.h"
#include "EST_Pathname.h"

#ifdef SUPPORT_WIN32AUDIO

#include <EST_system.h>

#ifdef SYSTEM_IS_UNIX

// Even if we think we are unix, this must be Win32 if  we are asked
// to support win32 audio, presumably under cygnus gunwin32 These definitions
// aren't in the gnuwin32 headers

#undef TRUE
#undef FALSE
/*  Changed for cygwin 1
  #include <Windows32/BASE.h>
  #define SND_MEMORY          0x0004  
  #define WAVE_FORMAT_PCM     1
*/
#include <windows.h>

/* This is no longer required
extern "C" {
WINBOOL STDCALL PlaySoundA(LPCSTR  pszSound, HMODULE hmod, DWORD fdwSound);
};

#define PlaySound PlaySoundA
*/

#endif 

int win32audio_supported = TRUE;

struct riff_header {
  char riff[4];
  int  file_size;
  char wave[4];
  char fmt[4];
  int  header_size;
  short sample_format;
  short n_channels;
  int  sample_rate;
  int bytes_per_second;
  short block_align;
  short bits_per_sample;
  char data[4];
  int data_size;
};

int play_win32audio_wave(EST_Wave &inwave, EST_Option &al)
{
    char *buffer = new char[sizeof(riff_header) + inwave.length()*inwave.num_channels() * sizeof(short)];

    struct riff_header *hdr = (struct riff_header *)buffer;
    char *data = buffer + sizeof(struct riff_header);

    strncpy(hdr->riff, "RIFF", 4);
    hdr->file_size = sizeof(riff_header) + inwave.length()*sizeof(short);
    strncpy(hdr->wave, "WAVE", 4);
    strncpy(hdr->fmt, "fmt ", 4);
    hdr->header_size = 16;
    hdr->sample_format = WAVE_FORMAT_PCM;
    hdr->n_channels = inwave.num_channels();
    hdr->sample_rate = inwave.sample_rate();
    hdr->bytes_per_second = hdr->sample_rate * hdr->n_channels * 2;
    hdr->block_align =  hdr->n_channels * 2;
    hdr->bits_per_sample = 16;
    strncpy(hdr->data, "data", 4);
    hdr->data_size = hdr->n_channels * 2 * inwave.num_samples();
  
    memcpy(data, inwave.values().memory(), hdr->n_channels * 2 * inwave.num_samples());
    PlaySound( buffer,
               NULL,
               SND_MEMORY);
    delete [] buffer;
    return 1;
}

#else
int win32audio_supported = FALSE;

int play_win32audio_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "Windows win32 audio not supported" << endl;
    return -1;
}

#endif
