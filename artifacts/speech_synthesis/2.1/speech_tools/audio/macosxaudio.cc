/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                         Copyright (c) 1996-2009                       */
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
/*                       Author :  Brian Foley                           */
/*                                 bfoley@compsoc.nuigalway.ie           */
/*                       Date   :  February 2004                         */
/*************************************************************************/
/*                       OSX 10.6 updates                                */
/*                       Author :  Rob Clark                             */
/*                                 robert@cstr.ed.ac.uk                  */
/*                       Date   :  Jan 2009                              */
/*=======================================================================*/
#include "EST_unix.h"
#include "EST_cutils.h"
#include "EST_Wave.h"
#include "EST_Option.h"
#include "audioP.h"

#if defined (SUPPORT_MACOSX_AUDIO)

#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <unistd.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

int macosx_supported = TRUE;

AudioUnit outau;
EST_SMatrix *waveMatrix;
UInt32 waveSize;
UInt32 waveIndex;
bool done;

OSStatus render_callback(void *inref,                            
                        AudioUnitRenderActionFlags *inflags,
                        const AudioTimeStamp *instamp,
                        UInt32 inbus,
                        UInt32 inframes,
                        AudioBufferList *ioData)
{

  // fill each channel with available audio data  

  UInt32 channels = ioData->mNumberBuffers;
  int totalNumberOfBytes = waveSize;
  int channelBytesLeft = totalNumberOfBytes - waveIndex;
  int bufferSize = ioData->mBuffers[0].mDataByteSize;

  if(channelBytesLeft > 0) {
    if(channelBytesLeft < bufferSize) {
      for(UInt32 i = 0; i < channels; ++i) {
        waveMatrix->copy_column((int)i, (int short*)ioData->mBuffers[i].mData, waveIndex/2, channelBytesLeft/2);
        memset((char*)ioData->mBuffers[i].mData + channelBytesLeft, 0, bufferSize - channelBytesLeft) ;
      }
      waveIndex += channelBytesLeft;
    } else {
      for(UInt32 i = 0; i < channels; ++i)
        waveMatrix->copy_column((int)i, (int short*)ioData->mBuffers[i].mData, waveIndex/2, bufferSize/2);
      waveIndex += bufferSize;
    }
  } else {
  	for(UInt32 i = 0; i < channels; ++i)
  		memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
    done = TRUE;
  }

    return noErr;
}


void  CreateDefaultAU()
{
    OSStatus err = noErr;

    // Open the default output unit
    ComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    Component comp = FindNextComponent(NULL, &desc);
    if (comp == NULL) { printf ("FindNextComponent\n"); return; }
    
    err = OpenAComponent(comp, &outau);
    if (comp == NULL) { printf ("OpenAComponent=%ld\n", long(err)); return; }

    // Set up render callback
    AURenderCallbackStruct input;
    input.inputProc = render_callback;
    input.inputProcRefCon = NULL;

    err = AudioUnitSetProperty (outau, 
                                kAudioUnitProperty_SetRenderCallback, 
                                kAudioUnitScope_Input,
                                0, 
                                &input, 
                                sizeof(input));
    if (err) { printf ("AudioUnitSetProperty-CB=%ld\n", long(err)); return; }
    
}

int play_macosx_wave(EST_Wave &inwave, EST_Option &al)
{
    OSStatus err;
    AudioStreamBasicDescription waveformat, outformat;
    UInt32 size = sizeof(AudioStreamBasicDescription);
    UInt32 running;
    
    CreateDefaultAU();
         
    // The EST_Wave structure will allow us to access individula channels 
    // so this is set up using kAudioFormatFlagIsNonInterleaved format.
    // Here the per packet and per frame info is per channel.
    waveformat.mSampleRate = (Float64)inwave.sample_rate();
    waveformat.mFormatID = kAudioFormatLinearPCM;
    waveformat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger 
                                    | kAudioFormatFlagsNativeEndian
                                    | kLinearPCMFormatFlagIsPacked
                                    | kAudioFormatFlagIsNonInterleaved;
    waveformat.mFramesPerPacket = 1;
    waveformat.mChannelsPerFrame = inwave.num_channels();
    waveformat.mBytesPerPacket = 2;
    waveformat.mBytesPerFrame = 2;
    waveformat.mBitsPerChannel = 16;
    
    err = AudioUnitSetProperty(outau, 
    				                   kAudioUnitProperty_StreamFormat, 
    				                   kAudioUnitScope_Input, 
    				                   0, 
    				                   &waveformat, 
    				                   size);
    if (err != noErr) {
                cerr << "Error setting input audio stream format." << endl;
                CloseComponent(outau);
                return -1;
    }
    
    err = AudioUnitGetProperty(outau, 
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Output,
                               0,
                               &outformat,
                               &size);
    if (err != noErr) {
        cerr << "Error getting output audio stream format." << endl;
        CloseComponent(outau);
        return -1;
    }
            
    err = AudioUnitInitialize(outau);
        if (err) { 
            printf ("AudioUnitInitialize=%ld\n", long(err)); 
            return -1;
        }
    
    // set up for playing
    waveSize = inwave.num_samples()*sizeof(short); 
    waveMatrix = &inwave.values();
    done = FALSE;
    waveIndex = 0;
    
    err = AudioOutputUnitStart(outau);
    if (err != noErr) {
        cerr << "Error starting audio outup: " << err << endl;
        CloseComponent(outau);
        return -1;
    }
    
    // Poll every 50ms whether the sound has stopped playing yet.
    // Probably not the best way of doing things.
    size = sizeof(UInt32);
    do {
        usleep(50 * 1000);
        err = AudioUnitGetProperty(outau, kAudioOutputUnitProperty_IsRunning,
                    kAudioUnitScope_Global, 0, &running, &size);
    } while (err == noErr && running && !done);
        
    CloseComponent (outau);
    
    return 1;
}

#else

int macosx_supported = FALSE;

int play_macosx_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "OS X Core Audio in not supported in this configuration." << endl;
    return -1;
}

#endif