#include "spush_streamdataprovider.h"
#include "common/log.h"
#include "streamdata_statistics.h"
#include "common/sleep.h"
#include "datapacket/mediadatapacket.h"
#include "resource/resource.h"



QnServerPushDataProvider::QnServerPushDataProvider(QnResource* dev ):
QnStreamDataProvider(dev)
{
}


void QnServerPushDataProvider::run()
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

        QnAbstractMediaDataPacketPtr data ( getNextData() );

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

		QnCompressedVideoDataPtr videoData ( ( data->dataType == QnAbstractMediaDataPacket::VIDEO) ? data.staticCast<QnCompressedVideoData>() : QnCompressedVideoDataPtr(0) );

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
					continue;
				}

				m_gotKeyFrame[videoData->channelNumber]++;
			}
			else
			{
				// need key data but got not key data
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
        {
            setNeedKeyData();
        }

        
	}

    if (isStreamOpened())
        closeStream();

	CL_LOG(cl_logINFO) cl_log.log("stream reader stopped.", cl_logINFO);
}
