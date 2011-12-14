
#include "utils/common/sleep.h"
#include "spush_media_stream_provider.h"


CLServerPushStreamreader::CLServerPushStreamreader(QnResourcePtr dev ):
QnAbstractMediaStreamDataProvider(dev)
{
}


void CLServerPushStreamreader::run()
{
	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("stream reader started."), cl_logINFO);

    beforeRun();

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
                QnSleep::msleep(20); // to avoid large CPU usage

                closeStream(); // to release resources 

                setNeedKeyData();
                mFramesLost++;
                m_stat[0].onData(0);
                m_stat[0].onEvent(CL_STAT_FRAME_LOST);

                if (mFramesLost==4) // if we lost 2 frames => connection is lost for sure (2)
                {
                    m_stat[0].onLostConnection();
                    getResource()->setStatus(QnResource::Offline);
                }

                continue;
            }
        }

        QnAbstractMediaDataPtr data = getNextData();

		if (data==0)
		{
			setNeedKeyData();
			mFramesLost++;
			m_stat[0].onData(0);
			m_stat[0].onEvent(CL_STAT_FRAME_LOST);

			if (mFramesLost==4) // if we lost 2 frames => connection is lost for sure (2)
            {
				m_stat[0].onLostConnection();
                getResource()->setStatus(QnResource::Offline);
            }

			continue;
		}

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
            m_stat[videoData->channelNumber].onData(data->data.size());
            ++m_framesSinceLastMetaData;
        }
        else if (qSharedPointerDynamicCast<QnMetaDataV1>(data))
        {
            m_framesSinceLastMetaData = 0;
            m_timeSinceLastMetaData.restart();
        }


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

    afterRun();

    CL_LOG(cl_logINFO) cl_log.log(QLatin1String("stream reader stopped."), cl_logINFO);
}
