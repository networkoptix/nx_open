#include "camdisplay.h"
#include "../data/mediadata.h"
#include "../device/device_video_layout.h"
#include "videostreamdisplay.h"
#include "audiostreamdisplay.h"
#include "../src/corelib/tools/qstringbuilder.h"

#define CL_MAX_DISPLAY_QUEUE_SIZE 7
#define AUDIO_BUFF_SIZE (4000) // ms

CLCamDisplay::CLCamDisplay():
CLAbstractDataProcessor(CL_MAX_DISPLAY_QUEUE_SIZE),
m_prev_video_displayed_time(0),
m_delay(100*1000), // do not put a big value here to avoid cpu usage in case of zoom out 
m_playAudio(false),
m_last_audio_packet_time(0),
m_last_video_packet_time(0),
m_hadAudio(false),
m_after_jump(false),
m_display_lasts(0)
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

void CLCamDisplay::addVideoChannel(int index, CLAbstractRenderer* vw, bool can_downsacle)
{
	Q_ASSERT(index<CL_MAX_CHANNELS);

	m_display[index] = new CLVideoStreamDisplay(can_downsacle);
	m_display[index]->setDrawer(vw);
}

void CLCamDisplay::display(CLCompressedVideoData* vd, bool sleep)
{

	// simple data provider/streamer/streamreader has the same delay, but who cares ?
	// to avoid cpu usage in case of a lot data in queue(zoomed out on the scene, lets add same delays here )
	quint64 curr_time = vd->timestamp;

	// in ideal world data comes to queue at the same speed as it goes out 
	// but timer on the sender side runs at a bit different rate in comparison to the timer here
	// adaptive delay will not solve all problems => need to minus little appendix based on queue size
	qint32 need_to_sleep = curr_time - m_prev_video_displayed_time - (m_dataQueue.size())*2*1000;

	m_prev_video_displayed_time = curr_time;

	//===== to avoid unrelated streams / stop play delays 
	if (need_to_sleep < 0)
		need_to_sleep = 0;

	if (need_to_sleep > 500*1000) // in case of key frame only and sliding archive slider forward - would not look good; need to do smth
		need_to_sleep = 0;
	//=========

    
	if (sleep)
		m_delay.sleep(need_to_sleep);
    

	int channel = vd->channel_num;

	CLVideoDecoderOutput::downscale_factor scale_factor = CLVideoDecoderOutput::factor_any;
	if (channel > 0) // if device has more than one channel 
	{
		// this here to avoid situation where different channels have different down scale factor; it might lead to diffrent size
		scale_factor = m_display[0]->getCurrentDownScaleFactor(); // [0] - master channel
	}

	if (m_display[channel])
    {
        // sometimes draw + decoding takes a lot of time. so to be able always sync video and audio we MUST not draw
        QTime display_time;
        display_time.restart();

        bool draw = sleep || (m_display_lasts*1000 < need_to_sleep); // do not draw if computer is very slow and we still wanna sync with audio


		m_display[channel]->dispay(vd, draw, scale_factor);

        if (!draw)
            cl_log.log("skip drawing frame!!", display_time.elapsed(), cl_logDEBUG1);


        if (!sleep)
            m_display_lasts = display_time.elapsed(); // this is how long would i take to draw frame.

        

        //m_display[channel]->dispay(vd, sleep, scale_factor);
        //cl_log.log(" video queue size = ", m_videoQueue[0].size(),  cl_logALWAYS);
    }

	vd->releaseRef();
}

void CLCamDisplay::jump()
{
    m_after_jump = true;
    clearUnProcessedData();
}

void CLCamDisplay::processData(CLAbstractData* data)
{

	CLCompressedVideoData *vd = 0;
	CLCompressedAudioData *ad = 0;

	CLAbstractMediaData* md = static_cast<CLAbstractMediaData*>(data);
	if (md->dataType == CLAbstractMediaData::VIDEO)
		vd = static_cast<CLCompressedVideoData*>(data);
	else if (md->dataType == CLAbstractMediaData::AUDIO)
		ad = static_cast<CLCompressedAudioData*>(data);

    if (m_after_jump)
    {
        m_after_jump = false;
        // clear everything we can
        after_jump((vd? vd->timestamp : ad->timestamp));
    }

	if (ad)
	{
        if (ad->timestamp < m_last_audio_packet_time)
            after_jump(ad->timestamp);
        m_last_audio_packet_time = ad->timestamp;

		// we synch video to the audio; so just put audio in player with out thinking
		if (m_playAudio)
		{
			m_audioDisplay->putdata(ad);
            m_hadAudio = true;
		}
        else if (m_hadAudio)
        {
            m_audioDisplay->enqueData(ad);
        }

        


	}
	else if (vd)
	{
        if (vd->timestamp < m_last_video_packet_time)
            after_jump(vd->timestamp);
        m_last_video_packet_time = vd->timestamp;

        if (haveAudio())
        {
            // to put data from ring buff to audio buff( if any )
            m_audioDisplay->putdata(0);
        }


		int channel = vd->channel_num;
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

			display(vd,true);
			return;
		}

		//2) we have audio and it's buffering( not playing yet )
		if (haveAudio() && m_audioDisplay->isBuffring())
		{
			// audio is not playinf yet; video must not be played as well
            enqueueVideo(vd);
			
			return;
		}

		//3) video and audio playing
		if (haveAudio() && !m_audioDisplay->isBuffring())
		{
            qint64 videoDuration;
            qint64 diff = diffBetweenVideoandAudio(vd, channel, videoDuration);

            //cl_log.log("diff = ", (int)diff/1000, cl_logALWAYS);

            if (diff >= 2*videoDuration) //factor 2 here is to avoid frequent switch between normal play and fast play
            {
                // video runs faster than audio; // need to hold video this frame
                enqueueVideo(vd);

                cl_log.log("HOLD FRAME", cl_logDEBUG1);

                return;
            }

            if (diff < 2*videoDuration) //factor 2 here is to avoid frequent switch between normal play and fast play
            {
                // need to draw frame(s); at least one

                CLCompressedVideoData* incoming = vd;

                int fast_frames = 0;

                while(1)
                {
                    qint64 diff = diffBetweenVideoandAudio(incoming, channel, videoDuration);

                    vd = nextInOutVideodata(incoming, channel);
                    incoming = 0;

                    if (!vd)
                        break; // no more video in queue


                    bool lastFrameToDisplay = qAbs(diff) < 2*videoDuration; //factor 2 here is to avoid frequent switch between normal play and fast play

                    display(vd, lastFrameToDisplay);

                    if (!lastFrameToDisplay)
                    {
                        cl_log.log("FAST PLAY, diff = ", (int)diff/1000, cl_logDEBUG1);
                        //cl_log.log("ms audio buff = ", m_audioDisplay->msInBuffer(), cl_logWARNING);

                        ++fast_frames;

                        if (fast_frames==2) // allow just 2 fast frames at the time
                            break;
                    }   

                    if (lastFrameToDisplay)
                        break;


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

void CLCamDisplay::coppyImage(bool copy)
{
	for (int i = 0; i< CL_MAX_CHANNELS; ++i)
		if (m_display[i])
			m_display[i]->coppyImage(copy);
}

void CLCamDisplay::playAudio(bool play)
{
	m_playAudio = play;

    if (!play)
    {
        m_audioDisplay->clearDeviceBuff();
    }
  
}

//==========================================================================

bool CLCamDisplay::haveAudio() const
{
	return m_playAudio && m_hadAudio;
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

qint64 CLCamDisplay::diffBetweenVideoandAudio(CLCompressedVideoData* incoming, int channel, qint64& duration) const
{
    qint64 current_paying_audio_time = m_last_audio_packet_time - (quint64)m_audioDisplay->msInBuffer()*1000;
    qint64 current_paying_video_time = nextVideoImageTime(incoming, channel);

    // strongly saning this duration of prev frame; not exact, but I think its fine
    // let's assume this frame has same duration
    duration = current_paying_video_time - m_prev_video_displayed_time;

    // difference between video and audio
    return current_paying_video_time - (current_paying_audio_time + 150*1000); // sorry for the magic number
}


void CLCamDisplay::enqueueVideo(CLCompressedVideoData* vd)
{
    if (m_videoQueue[vd->channel_num].size() < (AUDIO_BUFF_SIZE*30*5/1000)) // I assume we are not gonna buffer 
        m_videoQueue[vd->channel_num].enqueue(vd);
    else
        vd->releaseRef();
}

void CLCamDisplay::after_jump(qint64 new_time)
{
    cl_log.log("after jump", cl_logWARNING);

    m_last_audio_packet_time = new_time;
    m_prev_video_displayed_time = new_time;
    clearVideoQueue();
    m_audioDisplay->clearAudioBuff();
}