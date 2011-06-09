#include "camdisplay.h"
#include "../data/mediadata.h"
#include "../device/device_video_layout.h"
#include "videostreamdisplay.h"
#include "audiostreamdisplay.h"

#define CL_MAX_DISPLAY_QUEUE_SIZE 7
#define AUDIO_BUFF_SIZE (4000) // ms

CLCamDisplay::CLCamDisplay(bool generateEndOfStreamSignal)
    : CLAbstractDataProcessor(CL_MAX_DISPLAY_QUEUE_SIZE),
      m_previousVideoTime(0),
      m_lastNonZerroDuration(0),
      m_previousVideoDisplayedTime(0),
      m_delay(100*1000), // do not put a big value here to avoid cpu usage in case of zoom out 
      m_playAudio(false),
      m_lastAudioPacketTime(0),
      m_lastVideoPacketTime(0),
      m_hadAudio(false),
      m_afterJump(false),
      m_displayLasts(0),
      m_ignoringVideo(false),
      mGenerateEndOfStreamSignal(generateEndOfStreamSignal)
{
	for (int i = 0; i< CL_MAX_CHANNELS; ++i)
		m_display[i] = 0;

	m_audioDisplay = new CLAudioStreamDisplay(AUDIO_BUFF_SIZE);
}

CLCamDisplay::~CLCamDisplay()
{
	for (int i = 0; i< CL_MAX_CHANNELS; ++i) 
		delete m_display[i];

	clearVideoQueue();
	delete m_audioDisplay;
}

void CLCamDisplay::pause()
{
	CLAbstractDataProcessor::pause();

	m_audioDisplay->suspend();
}

void CLCamDisplay::resume()
{
	m_audioDisplay->resume();

	CLAbstractDataProcessor::resume();
}

void CLCamDisplay::addVideoChannel(int index, CLAbstractRenderer* vw, bool canDownscale)
{
	Q_ASSERT(index<CL_MAX_CHANNELS);

	m_display[index] = new CLVideoStreamDisplay(canDownscale);
	m_display[index]->setDrawer(vw);
}

void CLCamDisplay::display(CLCompressedVideoData* vd, bool sleep)
{
	// simple data provider/streamer/streamreader has the same delay, but who cares ?
	// to avoid cpu usage in case of a lot data in queue(zoomed out on the scene, lets add same delays here )
	quint64 currentTime = vd->timestamp;

	// in ideal world data comes to queue at the same speed as it goes out 
	// but timer on the sender side runs at a bit different rate in comparison to the timer here
	// adaptive delay will not solve all problems => need to minus little appendix based on queue size
	qint32 needToSleep = currentTime - m_previousVideoTime - (m_dataQueue.size()) * 2 * 1000;

	m_previousVideoTime = currentTime;

	//===== to avoid unrelated streams / stop play delays 
	if (needToSleep < 0)
		needToSleep = 0;

	if (needToSleep > 500 * 1000) // in case of key frame only and sliding archive slider forward - would not look good; need to do smth
		needToSleep = 0;
	//=========

	if (sleep)
		m_delay.sleep(needToSleep);

	int channel = vd->channelNumber;

	CLVideoDecoderOutput::downscale_factor scaleFactor = CLVideoDecoderOutput::factor_any;
	if (channel > 0) // if device has more than one channel 
	{
		// this here to avoid situation where different channels have different down scale factor; it might lead to diffrent size
		scaleFactor = m_display[0]->getCurrentDownscaleFactor(); // [0] - master channel
	}

	if (m_display[channel])
    {
        // sometimes draw + decoding takes a lot of time. so to be able always sync video and audio we MUST not draw
        QTime displayTime;
        displayTime.restart();

        bool draw = !vd->ignore && (sleep || (m_displayLasts * 1000 < needToSleep)); // do not draw if computer is very slow and we still wanna sync with audio

        // If there are multiple channels for this timestamp use only one of them
        if (channel == 0 && draw)
            m_previousVideoDisplayedTime = currentTime;

        CL_LOG(cl_logDEBUG2)
        {
            if (vd->ignore)
                cl_log.log("Ignoring frame ", (int)vd->timestamp, cl_logDEBUG2);
            else
                cl_log.log("Playing frame ", (int)vd->timestamp, cl_logDEBUG2);

            if (!draw)
                cl_log.log("skip drawing frame!!", displayTime.elapsed(), cl_logDEBUG2);

        }
        

		m_display[channel]->dispay(vd, draw, scaleFactor);

        if (!sleep)
            m_displayLasts = displayTime.elapsed(); // this is how long would i take to draw frame.

        //m_display[channel]->dispay(vd, sleep, scale_factor);
        //cl_log.log(" video queue size = ", m_videoQueue[0].size(),  cl_logALWAYS);
    }

	vd->releaseRef();
}

void CLCamDisplay::jump()
{
    m_afterJump = true;
    clearUnprocessedData();
}

void CLCamDisplay::processData(CLAbstractData* data)
{
	CLCompressedVideoData *vd = 0;
	CLCompressedAudioData *ad = 0;

	CLAbstractMediaData* md = static_cast<CLAbstractMediaData*>(data);
	if (md->dataType == CLAbstractMediaData::VIDEO)
    {
		vd = static_cast<CLCompressedVideoData*>(data);
        m_ignoringVideo = vd->ignore;
    }
	else if (md->dataType == CLAbstractMediaData::AUDIO)
		ad = static_cast<CLCompressedAudioData*>(data);

    if (m_afterJump)
    {
        m_afterJump = false;

        // clear everything we can
        afterJump(vd? vd->timestamp : ad->timestamp);
    }

	if (ad)
	{
        if (ad->timestamp < m_lastAudioPacketTime)
            afterJump(ad->timestamp);

        m_lastAudioPacketTime = ad->timestamp;

		// we synch video to the audio; so just put audio in player with out thinking
		if (m_playAudio)
		{
			m_audioDisplay->putData(ad);
            m_hadAudio = true;
		}
        else if (m_hadAudio)
        {
            m_audioDisplay->enqueueData(ad);
        }
	}
	else if (vd)
	{
        if (vd->timestamp < m_lastVideoPacketTime)
            afterJump(vd->timestamp);

        m_lastVideoPacketTime = vd->timestamp;

        if (haveAudio())
        {
            // to put data from ring buff to audio buff( if any )
            m_audioDisplay->putData(0);
        }

		int channel = vd->channelNumber;
		if (channel >= CL_MAX_CHANNELS)
			return;

		// this is the only point to addreff; 
		// video data can escape from this object only if displayed or in case of clearVideoQueue
		// so release ref must be in both places
		vd->addRef(); 

		// three are 3 possible scenarios:

		//1) we do not have audio playing;
		if (!haveAudio())
		{
			vd = nextInOutVideodata(vd, channel);
			if (!vd)
				return; // impossible? incoming vd!=0

			display(vd, !vd->ignore);
			return;
		}

		//2) we have audio and it's buffering( not playing yet )
		if (haveAudio() && m_audioDisplay->isBuffering())
		{
			// audio is not playinf yet; video must not be played as well
            enqueueVideo(vd);

			return;
		}

		//3) video and audio playing
		if (haveAudio() && !m_audioDisplay->isBuffering())
		{
            qint64 videoDuration;
            qint64 diff = diffBetweenVideoAndAudio(vd, channel, videoDuration);

            //cl_log.log("diff = ", (int)diff/1000, cl_logALWAYS);


            if (diff >= 2 * videoDuration) //factor 2 here is to avoid frequent switch between normal play and fast play
            {
                // video runs faster than audio; // need to hold video this frame
                enqueueVideo(vd);

                cl_log.log("HOLD FRAME", cl_logDEBUG1);

                return;
            }

            //if (diff < 2 * videoDuration) //factor 2 here is to avoid frequent switch between normal play and fast play
            else
            {
                // need to draw frame(s); at least one

                CLCompressedVideoData* incoming = vd;

                int fastFrames = 0;

                while(1)
                {
                    //qint64 diff = diffBetweenVideoAndAudio(incoming, channel, videoDuration);

                    vd = nextInOutVideodata(incoming, channel);
                    incoming = 0;

                    if (!vd)
                        break; // no more video in queue

                    bool lastFrameToDisplay = diff > -2 * videoDuration; //factor 2 here is to avoid frequent switch between normal play and fast play

                    display(vd, lastFrameToDisplay && !vd->ignore);

                    if (!lastFrameToDisplay)
                    {
                        cl_log.log("FAST PLAY, diff = ", (int)diff / 1000, cl_logDEBUG1);
                        //cl_log.log("ms audio buff = ", m_audioDisplay->msInBuffer(), cl_logWARNING);

                        ++fastFrames;

                        if (fastFrames == 2) // allow just 2 fast frames at the time
                            break;
                    }   

                    if (lastFrameToDisplay)
                    {
                        break;
                    }
                    diff = diffBetweenVideoAndAudio(incoming, channel, videoDuration);
                }
            }
		}
	}
}

void CLCamDisplay::setLightCPUMode(bool val)
{
	for (int i = 0; i< CL_MAX_CHANNELS; ++i)
		if (m_display[i])
			m_display[i]->setLightCPUMode(val);
}

void CLCamDisplay::copyImage(bool copy)
{
	for (int i = 0; i< CL_MAX_CHANNELS; ++i)
		if (m_display[i])
			m_display[i]->copyImage(copy);
}

void CLCamDisplay::playAudio(bool play)
{
	m_playAudio = play;

    if (!play)
    {
        m_audioDisplay->clearDeviceBuffer();
    }
}

//==========================================================================

bool CLCamDisplay::haveAudio() const
{
	return (m_playAudio && m_hadAudio) && !m_ignoringVideo && m_audioDisplay->isFormatSupported();
}

CLCompressedVideoData* CLCamDisplay::nextInOutVideodata(CLCompressedVideoData* incoming, int channel)
{
    if (!incoming && m_videoQueue[channel].isEmpty())
		return 0;

	if (m_videoQueue[channel].isEmpty())
		return incoming;

	// queue is not empty 
	if (incoming)
		enqueueVideo(incoming);
	return m_videoQueue[channel].dequeue();
}

quint64 CLCamDisplay::nextVideoImageTime(CLCompressedVideoData* incoming, int channel) const
{
    if (!incoming && m_videoQueue[channel].isEmpty())
        return 0;

    if (m_videoQueue[channel].isEmpty())
        return incoming->timestamp;

    // queue is not empty 
    return m_videoQueue[channel].head()->timestamp;

}

void CLCamDisplay::clearVideoQueue()
{
	for (int i = 0; i< CL_MAX_CHANNELS; ++i)
	{
		while (!m_videoQueue[i].isEmpty())
		{
			m_videoQueue[i].dequeue()->releaseRef();
		}
	}
}

qint64 CLCamDisplay::diffBetweenVideoAndAudio(CLCompressedVideoData* incoming, int channel, qint64& duration) 
{
    qint64 currentPlayingAudioTime = m_lastAudioPacketTime - (quint64)m_audioDisplay->msInBuffer()*1000;

    qint64 currentPlayingVideoTime = nextVideoImageTime(incoming, channel);

    // strongly saning this duration of prev frame; not exact, but I think its fine
    // let's assume this frame has same duration
    duration = currentPlayingVideoTime - m_previousVideoTime;
    if (duration == 0)
        duration = m_lastNonZerroDuration;
    else
        m_lastNonZerroDuration = duration;

    // difference between video and audio
    return currentPlayingVideoTime - currentPlayingAudioTime; 
}

void CLCamDisplay::enqueueVideo(CLCompressedVideoData* vd)
{
    if (m_videoQueue[vd->channelNumber].size() < (AUDIO_BUFF_SIZE*30*5/1000)) // I assume we are not gonna buffer 
        m_videoQueue[vd->channelNumber].enqueue(vd);
    else
        vd->releaseRef();
}

void CLCamDisplay::afterJump(qint64 newTime)
{
    cl_log.log("after jump", cl_logWARNING);

    m_lastAudioPacketTime = newTime;
    m_previousVideoTime = newTime;
    m_previousVideoDisplayedTime = 0;
    clearVideoQueue();
    m_audioDisplay->clearAudioBuffer();

    if (mGenerateEndOfStreamSignal)
        emit reachedTheEnd();
}
