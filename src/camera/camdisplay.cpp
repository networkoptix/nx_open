#include "camdisplay.h"
#include "../data/mediadata.h"
#include "../device/device_video_layout.h"

#define CL_MAX_DISPLAY_QUEUE_SIZE 10



CLCamDisplay::CLCamDisplay():
CLAbstractDataProcessor(CL_MAX_DISPLAY_QUEUE_SIZE),
m_prev_time(0),
m_delay(50) // do put a big value here to avoid cpu usage in case of zoom out 
{
	for (int i = 0; i< CL_MAX_CHANNELS; ++i)
		m_display[i] = 0;
}

CLCamDisplay::~CLCamDisplay()
{
	for (int i = 0; i< CL_MAX_CHANNELS; ++i)
		delete m_display[i];
}

void CLCamDisplay::addVideoChannel(int index, CLAbstractRenderer* vw)
{
	Q_ASSERT(index<CL_MAX_CHANNELS);

	m_display[index] = new CLVideoStreamDisplay();
	m_display[index]->setDrawer(vw);
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

		// simple data provider/streamer/streamreader has the same delay, bu who cares ?
		// to avoid cpu usage in case of a lot data in queue(zoomed out on the scene, lets add same dalays here )

		quint64 curr_time = vd->timestamp;

		// in ideal world data comes to queue at the same speed as it goes out 
		// but timer on the sender side runs at a bit different rate in comparison to the timer here
		// adaptive delay will not solve all problems => need to minus little appendix based on queue size
		qint32 need_to_sleep = curr_time - m_prev_time - (m_dataQueue.size()+1)/2;

		m_prev_time = curr_time;

		//===== to avoid unrelated strems / stop play delays 
		if (need_to_sleep<0)
			need_to_sleep = 0;

		if (need_to_sleep>1000)
			need_to_sleep = 2;
		//=========


		m_delay.sleep(need_to_sleep);



		int channel = vd->channel_num;

		if (m_display[channel])	
			m_display[channel]->dispay(vd);
	}
	else if (ad)
	{

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