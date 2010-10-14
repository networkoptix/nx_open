#include "camdisplay.h"
#include "../data/mediadata.h"
#include "../device/device_video_layout.h"

#define CL_MAX_DISPLAY_QUEUE_SIZE 3



CLCamDisplay::CLCamDisplay():
CLAbstractDataProcessor(CL_MAX_DISPLAY_QUEUE_SIZE)
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
	CLCompressedVideoData *vd= static_cast<CLCompressedVideoData*>(data);

	int channel = vd->channel_num;

	if (m_display[channel])	
		m_display[channel]->dispay(vd);

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