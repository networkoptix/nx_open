#include "utils/common/sleep.h"
#include "cpull_media_stream_provider.h"


QnClientPullStreamProvider::QnClientPullStreamProvider(QnResource* dev ):
QnAbstractMediaStreamDataProvider(dev)
{
}


void QnClientPullStreamProvider::run()
{
	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("stream reader started."), cl_logINFO);

	setNeedKeyData();
	m_resource->beforeUse();

	int frames_lost = 0;

	while(!needToStop())
	{
		pauseDelay(); // pause if needed;
		if (needToStop()) // extra check after pause
			break;

		// check queue sizes

		if (!dataCanBeAccepted())
		{
			QnSleep::msleep(5);
			continue;
		}


		if (QnResource::commandProchasSuchDeviceInQueue(m_resource->getUniqueId())) // if command processor has something in the queue for this device let it go first
		{
			QnSleep::msleep(5);
			continue;
		}

		QnAbstractDataPacketPtr data = getNextData();
		//continue;

		if (data==0)
		{
			setNeedKeyData();
			frames_lost++;
			m_stat[0].onData(0);
			m_stat[0].onEvent(CL_STAT_FRAME_LOST);

			if (frames_lost==4) // if we lost 2 frames => connection is lost for sure (2)
				m_stat[0].onLostConnection();

			QnSleep::msleep(30);

			continue;
		}

		QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(data);

		if (frames_lost>0) // we are alive again
		{
			if (frames_lost>=4)
			{
				m_stat[0].onEvent(CL_STAT_CAMRESETED);

				m_resource->beforeUse();
			}

			frames_lost = 0;
		}

		if (videoData && needKeyData())
		{
			// I do not like; need to do smth with it
			if (videoData->flags & AV_PKT_FLAG_KEY)
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
            m_stat[videoData->channelNumber].onData(videoData->data.size());

		putData(data);
	}

	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("stream reader stopped."), cl_logINFO);
}
