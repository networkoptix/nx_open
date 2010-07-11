#include "./cpull_stremreader.h"
#include "../base/sleep.h"
#include "../base/log.h"
#include "../data/mediadata.h"
#include "device/device.h"
#include "device/device_video_layout.h"

CLClientPullStreamreader::CLClientPullStreamreader(CLDevice* dev ):
CLStreamreader(dev)
{
	for (int i = 0; i < CL_MAX_CHANNEL_NUMBER; ++i)
		m_key_farme_helper[i] = false;

	m_channel_number = dev->getVideoLayout()->numberOfChannels();

}

void CLClientPullStreamreader::needKeyData()
{
	m_needKeyData = true;
	for (int i = 0; i < m_channel_number; ++i)
		m_key_farme_helper[i] = false;

}

bool CLClientPullStreamreader::isKeyDataReceived() const
{
	for (int i = 0; i < m_channel_number; ++i)
		if (!m_key_farme_helper[i])
			return false;

	return true;

}

bool CLClientPullStreamreader::needToRead() const
{
	// need to read only if all queues has more space and at least one queue is exist
	bool result = false;
	for (int i = 0; i < m_dataprocessors.size(); ++i)
	{
		CLAbstractDataProcessor* dp = m_dataprocessors.at(i);
		
		 if (dp->canAcceptData())
			result = true;
		else 
			return false;
		
	}
	return result;
}

void CLClientPullStreamreader::run()
{

	CL_LOG(cl_logINFO) cl_log.log("stream reader started.", cl_logINFO);

	m_needKeyData = true;
	int frames_lost = 0;

	while(!needToStop())
	{
		// check queue sizes

		if (!needToRead())
		{
			CLSleep::msleep(5);
			continue;
		}


		CLAbstractMediaData *data = 0;
		data = getNextData();
		//if (data) data->releaseRef();
		//continue;
		
		if (data==0)
		{
			m_needKeyData = true;
			frames_lost++;
			m_stat[0].onData(0);
			m_stat[0].onEvent(CL_STAT_FRAME_LOST);

			if (frames_lost==2) // if we lost 2 frames => connection is lost for sure (2)
				m_stat[0].onLostConnection();

			CLSleep::msleep(20);
			
			continue;
		}

		if (frames_lost>0) // we are alive again
		{
			if (frames_lost>=2)
			{
				m_stat[0].onEvent(CL_STAT_CAMRESETED);

				if (m_restartHandler) // if we have handler
					m_restartHandler->onDeviceRestarted(this, m_restartInfo);
			}

			frames_lost = 0;
		}

		
		

		if (m_needKeyData)
		{
			// I do not like; need to do smth with it 
			CLCompressedVideoData *comp_data = static_cast<CLCompressedVideoData *>(data);
			if (comp_data->keyFrame)
			{
				m_key_farme_helper[data->channel_num] = true;

				if (isKeyDataReceived())
					m_needKeyData = false;
			}
		}


		putData(data);

		// put it in queue
		//m_dataqueue[channel_num]->push(data);

		m_stat[data->channel_num].onData(data->data.size());

	}

	CL_LOG(cl_logINFO) cl_log.log("stream reader stopped.", cl_logINFO);
}
