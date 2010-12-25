#include "./cpull_stremreader.h"
#include "../base/sleep.h"
#include "../base/log.h"
#include "../data/mediadata.h"
#include "device/device.h"
#include "device/device_video_layout.h"

CLClientPullStreamreader::CLClientPullStreamreader(CLDevice* dev ):
CLStreamreader(dev)
{

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

	setNeedKeyData();
	m_device->onBeforeStart();

	int frames_lost = 0;

	while(!needToStop())
	{

		pause_delay(); // pause if needed;
		if (needToStop()) // extra check after pause
			break;

		// check queue sizes

		if (!needToRead())
		{
			CLSleep::msleep(5);
			continue;
		}

		if (CLDevice::commandProchasSuchDeviceInQueue(m_device)) // if command processor has something in the queue for this device let it go first
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
			setNeedKeyData();
			frames_lost++;
			m_stat[0].onData(0);
			m_stat[0].onEvent(CL_STAT_FRAME_LOST);

			if (frames_lost==2) // if we lost 2 frames => connection is lost for sure (2)
				m_stat[0].onLostConnection();

			CLSleep::msleep(30);
			
			continue;
		}


		CLCompressedVideoData* videoData = ( data->dataType == CLAbstractMediaData::VIDEO) ? static_cast<CLCompressedVideoData *>(data) : 0;


		if (frames_lost>0) // we are alive again
		{
			if (frames_lost>=2)
			{
				m_stat[0].onEvent(CL_STAT_CAMRESETED);

				m_device->onBeforeStart();
			}

			frames_lost = 0;
		}

		
		

		if (videoData && needKeyData())
		{
			// I do not like; need to do smth with it 
			if (videoData->keyFrame)
			{
				if (videoData->channel_num>CL_MAX_CHANNEL_NUMBER-1)
				{
					Q_ASSERT(false);
					data->releaseRef();
					continue;
				}



				m_gotKeyFrame[videoData->channel_num]++;

			}
			else
			{
				// need key data but got not key data
				data->releaseRef();
				continue;
			}
		}

		data->dataProvider = this;

		putData(data);

		// put it in queue
		//m_dataqueue[channel_num]->push(data);

		if (videoData)
			m_stat[videoData->channel_num].onData(data->data.size());

	}

	CL_LOG(cl_logINFO) cl_log.log("stream reader stopped.", cl_logINFO);
}
