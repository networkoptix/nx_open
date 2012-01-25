#include "utils/common/sleep.h"
#include "cpull_media_stream_provider.h"

QnClientPullMediaStreamProvider::QnClientPullMediaStreamProvider(QnResourcePtr dev ):
    QnAbstractMediaStreamDataProvider(dev),
    m_fpsSleep(100*1000)
{
}


void QnClientPullMediaStreamProvider::run()
{
    setPriority(QThread::HighPriority);
	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("stream reader started."), cl_logINFO);

    beforeRun();

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


		if (QnResource::commandProcHasSuchResourceInQueue(m_resource)) // if command processor has something in the queue for this device let it go first
		{
			QnSleep::msleep(5);
			continue;
		}

		QnAbstractMediaDataPtr data = getNextData();
		//continue;

		if (data==0)
		{
            if (m_needStop)
                continue;

			setNeedKeyData();
			mFramesLost++;
			m_stat[0].onData(0);
			m_stat[0].onEvent(CL_STAT_FRAME_LOST);

			if (mFramesLost % 4 == 0) // if we lost 4 frames => connection is lost for sure (4)
            {
                getResource()->setStatus(QnResource::Offline);
				m_stat[0].onLostConnection();
            }

            /*
            if (getResource()->getStatus() == QnResource::Offline) {
                for (int i = 0; i < 100 && !m_needStop; ++i)
                    QnSleep::msleep(10);
            }
            else */
            {
                QnSleep::msleep(30);
            }

			continue;
		}

        
        
        if (getResource()->checkFlag(QnResource::local_live_cam)) // for all local live cam add MediaFlags_LIVE flag; 
            data->flags |= QnAbstractMediaData::MediaFlags_LIVE;


        getResource()->setStatus(QnResource::Online);

		QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(data);
        

		if (mFramesLost>0) // we are alive again
		{
			if (mFramesLost>=4)
			{
				m_stat[0].onEvent(CL_STAT_CAMRESETED);
			}

			mFramesLost = 0;
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
        {
            m_stat[videoData->channelNumber].onData(videoData->data.size());
            ++m_framesSinceLastMetaData;
        }
        else if (qSharedPointerDynamicCast<QnMetaDataV1>(data))
        {
            m_framesSinceLastMetaData = 0;
            m_timeSinceLastMetaData.restart();
        }

		putData(data);

        if (videoData && !isMaxFps())
            m_fpsSleep.sleep(1000*1000/getFps());

	}

    afterRun();

	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("stream reader stopped."), cl_logINFO);
}
