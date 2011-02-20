#include "camdisplay.h"
#include "../data/mediadata.h"
#include "../device/device_video_layout.h"
#include "videostreamdisplay.h"
#include "audiostreamdisplay.h"
#include "../src/corelib/tools/qstringbuilder.h"

#define CL_MAX_DISPLAY_QUEUE_SIZE 7
#define AUDIO_BUFF_SIZE (9000) // ms
#define AUDIO_BUFF_ACC_SIZE 2500 // ms

CLCamDisplay::CLCamDisplay():
CLAbstractDataProcessor(CL_MAX_DISPLAY_QUEUE_SIZE),
m_prev_time(0),
m_delay(100*1000), // do not put a big value here to avoid cpu usage in case of zoom out 
m_playAudioSet(false),
m_playAudio(false),
m_audioClock(0),
m_videoClock(0),
m_audioDuration(0)
{
	for (int i = 0; i< CL_MAX_CHANNELS; ++i)
		m_display[i] = 0;

	m_audioDisplay = new CLAudioStreamDisplay(AUDIO_BUFF_SIZE, AUDIO_BUFF_ACC_SIZE);
}

CLCamDisplay::~CLCamDisplay()
{
	for (int i = 0; i< CL_MAX_CHANNELS; ++i) 
		delete m_display[i];

	delete m_audioDisplay;

	clearVideoQueue();
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

void CLCamDisplay::addVideoChannel(int index, CLAbstractRenderer* vw, bool can_downsacle)
{
	Q_ASSERT(index<CL_MAX_CHANNELS);

	m_display[index] = new CLVideoStreamDisplay(can_downsacle);
	m_display[index]->setDrawer(vw);
}

void CLCamDisplay::display(CLCompressedVideoData* vd)
{
	int channel = vd->channel_num;

	CLVideoDecoderOutput::downscale_factor scale_factor = CLVideoDecoderOutput::factor_any;
	if (channel > 0) // if device has more than one channel 
	{
		// this here to avoid situation where dirrerent channels have different down scale factor; it might lead to diffrent size
		scale_factor = m_display[0]->getCurrentDownScaleFactor(); // [0] - master channel
	}

	if (m_display[channel])		
		m_display[channel]->dispay(vd, scale_factor);
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

	if (vd)
	{
		data->addRef();

		int channel = vd->channel_num;

		// Do not playing audio
		if (!m_playAudio || !m_audioDisplay->is_initialized())
		{
			if (!m_videoQueue[channel].isEmpty())
			{
				m_videoQueue[channel].enqueue(vd);
				vd = m_videoQueue[channel].dequeue();
			}

			display(vd);

			vd->releaseRef();

			return;
		}

		if (m_playAudioSet)
		{
			m_playAudioSet = false;
		}

		if (vd->timestamp < m_videoClock || vd->afterJump)
		{
			// Clear video queue and audio buffers
			m_audioDisplay->clear();
			clearVideoQueue();
		}

		m_videoClock = vd->timestamp;

		m_videoQueue[channel].enqueue(vd);

		// Hold this frame
		qint64 mksBuffered = 1000 * m_audioDisplay->msBuffered();
		m_isBuffering = (m_videoClock < m_audioClock + AUDIO_BUFF_ACC_SIZE * 1000 - mksBuffered);
		if (m_isBuffering)
			return;

		vd = m_videoQueue[channel].dequeue();

		// simple data provider/streamer/streamreader has the same delay, but who cares ?
		// to avoid cpu usage in case of a lot data in queue(zoomed out on the scene, lets add same delays here )

		quint64 curr_time = vd->timestamp;

		// in ideal world data comes to queue at the same speed as it goes out 
		// but timer on the sender side runs at a bit different rate in comparison to the timer here
		// adaptive delay will not solve all problems => need to minus little appendix based on queue size
		qint32 need_to_sleep = curr_time - m_prev_time - (m_dataQueue.size())*2*1000;

		m_prev_time = curr_time;

		//===== to avoid unrelated streams / stop play delays 
		if (need_to_sleep < 0)
			need_to_sleep = 0;

		if (need_to_sleep > 500*1000) // in case of key frame only and sliding archive slider forward - would not look good; need to do smth
			need_to_sleep = 0;
		//=========


		m_delay.sleep(need_to_sleep);

		m_audioDisplay->putdata(0);

		display(vd);

		vd->releaseRef();

		while (!m_videoQueue[channel].isEmpty())
		{
			vd = m_videoQueue[channel].head();

			int mksBuffered = 1000 * m_audioDisplay->msBuffered();
			if (vd->timestamp + m_videoDuration  > m_audioClock - mksBuffered)
			{
				break;
			}

			cl_log.log("ADDING FRAME", cl_logALWAYS);

			vd = m_videoQueue[channel].dequeue();

			display(vd);

			vd->releaseRef();
		}
	}
	else if (ad)
	{
		if (ad->timestamp > m_audioClock)
			m_audioDuration = ad->timestamp - m_audioClock;

		m_audioClock = ad->timestamp + m_audioDuration;

		if (m_playAudio)
			m_audioDisplay->putdata(ad);
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
	if (m_playAudio == false && play == true)
	{
		m_playAudioSet = true;
	}

	m_playAudio = play;
}