
#include "utils/common/sleep.h"
#include "spush_media_stream_provider.h"


CLServerPushStreamreader::CLServerPushStreamreader(QnResourcePtr dev ):
QnAbstractMediaStreamDataProvider(dev)
{
}


void CLServerPushStreamreader::run()
{
	CL_LOG(cl_logINFO) cl_log.log(QLatin1String("stream reader started."), cl_logINFO);

	setNeedKeyData();

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
                QnSleep::msleep(20); // to avoid large CPU usage

                closeStream(); // to release resources 

                setNeedKeyData();
                frames_lost++;
                m_stat[0].onData(0);
                m_stat[0].onEvent(CL_STAT_FRAME_LOST);

                if (frames_lost==4) // if we lost 2 frames => connection is lost for sure (2)
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
			frames_lost++;
			m_stat[0].onData(0);
			m_stat[0].onEvent(CL_STAT_FRAME_LOST);

			if (frames_lost==4) // if we lost 2 frames => connection is lost for sure (2)
            {
				m_stat[0].onLostConnection();
                getResource()->setStatus(QnResource::Offline);
            }

			continue;
		}

        getResource()->setStatus(QnResource::Online);

		QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(data);

		if (frames_lost>0) // we are alive again
		{
			if (frames_lost>=4)
			{
				m_stat[0].onEvent(CL_STAT_CAMRESETED);
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

    CL_LOG(cl_logINFO) cl_log.log(QLatin1String("stream reader stopped."), cl_logINFO);
}
