/* Check all of ./speech_tools and ./festival for reference to win32audio, */
/* and add the equivalent for the os2audio module.  You will need -los2me  */
/* to compile this os2audio module. */


/* OS/2 16bit realtime DART Audio support for Festival                      */
/*                                          Samuel Audet <guardia@cam.org>  */
/* DeviceID is setup in here.  Can be used as an external variable!         */

#ifdef SUPPORT_OS2AUDIO 

#include "EST_cutils.h" 

#include "EST_Wave.h" 

#include "EST_Option.h" 

#include "audioP.h" 

#include "EST_io_aux.h" 

#include "EST_unix.h" 

int os2audio_supported = TRUE; 

#define INCL_OS2MM

#define INCL_DOS

#include <os2.h> 

#include <os2me.h> 


USHORT DeviceID = 0;  /* 0 = default sound device, 1 = 1st sound device, etc. */

/* new stuff */
#define MIX_BUFFER_EOS  1


#define NUM_BUFFERS 8        /* Number of audio buffer usable */
static ULONG ulMCIBuffers;   /* warning, minimum 2 */

static MCI_AMP_OPEN_PARMS  maop;
static MCI_MIXSETUP_PARMS  mmp;
static MCI_BUFFER_PARMS    mbp;
static MCI_GENERIC_PARMS   mgp = {0};
static MCI_MIX_BUFFER      MixBuffers[NUM_BUFFERS];
static unsigned char *wave, *startwave, *endwave;
static ULONG sizewave;

static HEV dataplayed;


static void MciError(ULONG ulError)
{
   unsigned char buffer[128];
   unsigned char string[128];
   ULONG rc;

   rc = mciGetErrorString(ulError, buffer, sizeof(buffer));

   if (rc == MCIERR_SUCCESS)
      sprintf(string,"MCI Error %d: %s",ULONG_LOWD(ulError),buffer);
   else
      sprintf(string,"MCI Error %d: Cannot query error message.",ULONG_LOWD(rc));

   cerr << string << endl;
}


static LONG APIENTRY MyEvent(ULONG ulStatus, PMCI_MIX_BUFFER PlayedBuffer, ULONG ulFlags)
{
   if(PlayedBuffer->ulFlags == MIX_BUFFER_EOS)
      DosPostEventSem(dataplayed);
   else
   switch(ulFlags)
   {
   case MIX_STREAM_ERROR | MIX_WRITE_COMPLETE:  /* error occur in device */

      if ( ulStatus == ERROR_DEVICE_UNDERRUN)
         /* Write buffers to rekick off the amp mixer. */
         mmp.pmixWrite( mmp.ulMixHandle,
                        MixBuffers,
                        ulMCIBuffers );
      break;

   case MIX_WRITE_COMPLETE:                     /* for playback  */

      if(wave < endwave)
      {
         /* copy new data in the played buffer */
         if(wave + PlayedBuffer->ulBufferLength >= endwave)
         {
            PlayedBuffer->ulFlags = MIX_BUFFER_EOS; /* set end of stream */
            PlayedBuffer->ulBufferLength = (ULONG) (endwave - wave);
         }
         memcpy(PlayedBuffer->pBuffer, wave, PlayedBuffer->ulBufferLength);
         wave += PlayedBuffer->ulBufferLength;

         mmp.pmixWrite( mmp.ulMixHandle,
                        PlayedBuffer /* contains new data */,
                        1 );
      }

      break;

   } /* end switch */

   return( TRUE );

} /* end MyEvent */


int play_os2audio_wave(EST_Wave &inwave, EST_Option &al)
{
   ULONG i,rc;

   startwave = (unsigned char *) inwave.values().memory();
   sizewave = inwave.num_samples()*sizeof(short)*inwave.num_channels();
   endwave = startwave + sizewave;
   wave = startwave;

   /* open the mixer device */
   memset (&maop, 0, sizeof(maop));
   maop.usDeviceID = 0;
   maop.pszDeviceType = (PSZ) MAKEULONG(MCI_DEVTYPE_AUDIO_AMPMIX, DeviceID);

   rc = mciSendCommand(0,
                       MCI_OPEN,
                       MCI_WAIT | MCI_OPEN_TYPE_ID | MCI_OPEN_SHAREABLE,
                       &maop,
                       0);

   if (rc != MCIERR_SUCCESS)
   {
      MciError(rc);
      return(-1);
   }

   /* Set the MCI_MIXSETUP_PARMS data structure to match the audio stream. */

   memset(&mmp, 0, sizeof(mmp));

   mmp.ulBitsPerSample = 16;
   mmp.ulFormatTag = MCI_WAVE_FORMAT_PCM;
   mmp.ulSamplesPerSec = inwave.sample_rate();
   mmp.ulChannels = inwave.num_channels();

   /* Setup the mixer for playback of wave data  */
   mmp.ulFormatMode = MCI_PLAY;
   mmp.ulDeviceType = MCI_DEVTYPE_WAVEFORM_AUDIO;
   mmp.pmixEvent    = MyEvent;

   rc = mciSendCommand( maop.usDeviceID,
                        MCI_MIXSETUP,
                        MCI_WAIT | MCI_MIXSETUP_INIT,
                        &mmp,
                        0 );

   if ( rc != MCIERR_SUCCESS )
   {
      MciError(rc);
      return(-1);
   }

   /* Set up the BufferParms data structure and allocate
    * device buffers from the Amp-Mixer  */

   ulMCIBuffers = NUM_BUFFERS;
   mbp.ulNumBuffers = ulMCIBuffers;
   mbp.ulBufferSize = mmp.ulBufferSize;
   memset(MixBuffers, 0, sizeof(MixBuffers[0])*NUM_BUFFERS);
   mbp.pBufList = MixBuffers;

   rc = mciSendCommand( maop.usDeviceID,
                        MCI_BUFFER,
                        MCI_WAIT | MCI_ALLOCATE_MEMORY,
                        (PVOID) &mbp,
                        0 );

   if ( ULONG_LOWD(rc) != MCIERR_SUCCESS )
   {
      MciError(rc);
      return(-1);
   }

   ulMCIBuffers = mbp.ulNumBuffers; /* never know! */

   /* Fill all device buffers with data from the wave stream.  */

   for(i = 0; i < ulMCIBuffers; i++)
   {
      if(wave < endwave)
      {
         MixBuffers[i].ulFlags = 0;
         MixBuffers[i].ulBufferLength = mbp.ulBufferSize;
         if(wave + MixBuffers[i].ulBufferLength >= endwave)
         {
            MixBuffers[i].ulFlags = MIX_BUFFER_EOS; /* set end of stream */
            MixBuffers[i].ulBufferLength = (ULONG) (endwave - wave);
         }
         memcpy(MixBuffers[i].pBuffer, wave, MixBuffers[i].ulBufferLength);
         wave += MixBuffers[i].ulBufferLength;
      }
   }

   /* Create a semaphore to know when data has been played by the DART thread */
   DosCreateEventSem(NULL,&dataplayed,0,FALSE); 

   /* Write buffers to kick off the amp mixer. */
   rc = mmp.pmixWrite( mmp.ulMixHandle,
                       MixBuffers,
                       ulMCIBuffers );

   if ( rc != MCIERR_SUCCESS )
   {
      MciError(rc);
      return(-1);
   }

   DosWaitEventSem(dataplayed, -1);
   DosCloseEventSem(dataplayed);

   rc = mciSendCommand( maop.usDeviceID,
                        MCI_BUFFER,
                        MCI_WAIT | MCI_DEALLOCATE_MEMORY,
                        &mbp,
                        0 );

   if ( rc != MCIERR_SUCCESS )
   {
      MciError(rc);
      return(-1);
   }

   rc = mciSendCommand( maop.usDeviceID,
                        MCI_CLOSE,
                        MCI_WAIT ,
                        &mgp,
                        0 );

   if ( rc != MCIERR_SUCCESS )
   {
      MciError(rc);
      return(-1);
   }

   return TRUE;
}

#else 

# include "EST_Wave.h"
# include "EST_Option.h"

int os2audio_supported = FALSE;

int play_os2audio_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "OS/2 16bit realtime DART playback not supported." << endl;
    return -1;
}

#endif


