#include "./spush_streamreader.h"
#include "../base/sleep.h"
#include "../base/log.h"
#include "../data/mediadata.h"
#include "device/device.h"
#include "device/device_video_layout.h"

CLServerPushStreamreader::CLServerPushStreamreader(CLDevice* dev ):
CLStreamreader(dev)
{
}


void CLServerPushStreamreader::run()
{
	CL_LOG(cl_logINFO) cl_log.log("stream reader started.", cl_logINFO);

	setNeedKeyData();
	m_device->onBeforeStart();

	int frames_lost = 0;

	while(!needToStop())
	{
		pauseDelay(); // pause if needed;
		if (needToStop()) // extra check after pause
			break;

        if (!isStreamOpened())
        {
            openStream();
            if (!isStreamOpened())
            {
                CLSleep::msleep(20); // to avoid large CPU usage

                setNeedKeyData();
                frames_lost++;
                m_stat[0].onData(0);
                m_stat[0].onEvent(CL_STAT_FRAME_LOST);

                if (frames_lost==4) // if we lost 2 frames => connection is lost for sure (2)
                    m_stat[0].onLostConnection();

                continue;
            }
        }

        CLAbstractMediaData *data = 0;
        data = getNextData();

		if (data==0)
		{
			setNeedKeyData();
			frames_lost++;
			m_stat[0].onData(0);
			m_stat[0].onEvent(CL_STAT_FRAME_LOST);

			if (frames_lost==4) // if we lost 2 frames => connection is lost for sure (2)
				m_stat[0].onLostConnection();

			continue;
		}

		CLCompressedVideoData* videoData = ( data->dataType == CLAbstractMediaData::VIDEO) ? static_cast<CLCompressedVideoData *>(data) : 0;

		if (frames_lost>0) // we are alive again
		{
			if (frames_lost>=4)
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
				if (videoData->channelNumber>CL_MAX_CHANNEL_NUMBER-1)
				{
					Q_ASSERT(false);
					data->releaseRef();
					continue;
				}

				m_gotKeyFrame[videoData->channelNumber]++;
			}
			else
			{
				// need key data but got not key data
				data->releaseRef();
				continue;
			}
		}

		data->dataProvider = this;

        if (videoData)
            m_stat[videoData->channelNumber].onData(data->data.size());

        // check queue sizes
        if (dataCanBeAccepted())
            putData(data);        
        else
            data->releaseRef();

        
	}

    if (isStreamOpened())
        closeStream();

	CL_LOG(cl_logINFO) cl_log.log("stream reader stopped.", cl_logINFO);
}
