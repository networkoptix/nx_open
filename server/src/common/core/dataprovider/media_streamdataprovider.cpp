#include "media_streamdataprovider.h"



QnAbstractMediaStreamDataProvider::QnAbstractMediaStreamDataProvider(QnResourcePtr res):
QnAbstractStreamDataProvider(res),
m_qulity(CLSLowest),
m_frames_lost(0)
{
	memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
	m_NumaberOfVideoChannels = 0;//dev->getVideoLayout()->numberOfChannels(); //todo

}

QnAbstractMediaStreamDataProvider::~QnAbstractMediaStreamDataProvider()
{
	stop();
}



void QnAbstractMediaStreamDataProvider::setQuality(StreamQuality q)
{
    QMutexLocker mtx(&m_mtx);
	m_qulity = q;
}

QnAbstractMediaStreamDataProvider::StreamQuality QnAbstractMediaStreamDataProvider::getQuality() const
{
    QMutexLocker mtx(&m_mtx);
	return m_qulity;
}

void QnAbstractMediaStreamDataProvider::setNeedKeyData()
{
    QMutexLocker mtx(&m_mtx);
	for (int i = 0; i < m_NumaberOfVideoChannels; ++i)
		m_gotKeyFrame[i] = 0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData(int channel) const
{
    QMutexLocker mtx(&m_mtx);
	return m_gotKeyFrame[channel]==0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData() const
{
    QMutexLocker mtx(&m_mtx);
	for (int i = 0; i < m_NumaberOfVideoChannels; ++i)
		if (m_gotKeyFrame[i]==0)
			return true;

	return false;
}


void QnAbstractMediaStreamDataProvider::beforeRun()
{
    setNeedKeyData();
    getResource()->beforeUse();
    mFramesLost = 0;
}

void QnAbstractMediaStreamDataProvider::afterRun()
{

}


bool QnAbstractMediaStreamDataProvider::afterGetData(QnAbstractDataPacketPtr d)
{

    QnAbstractMediaDataPacketPtr data = d.staticCast<QnAbstractMediaDataPacket>();

    if (data==0)
    {
        setNeedKeyData();
        ++mFramesLost;
        m_stat[0].onData(0);
        m_stat[0].onEvent(CL_STAT_FRAME_LOST);

        if (mFramesLost == 4) // if we lost 2 frames => connection is lost for sure (2)
            m_stat[0].onLostConnection();

        QnSleep::msleep(10);

        return false;
    }

    QnCompressedVideoDataPtr videoData ( ( data->dataType == QnAbstractMediaDataPacket::VIDEO) ? data.staticCast<QnCompressedVideoData>() : QnCompressedVideoDataPtr(0)) ;

    if (mFramesLost > 0) // we are alive again
    {
        if (mFramesLost >= 4)
        {
            m_stat[0].onEvent(CL_STAT_CAMRESETED);

            getResource()->beforeUse(); // must be resource reseted
        }

        mFramesLost = 0;
    }

    if (videoData && needKeyData())
    {
        // I do not like; need to do smth with it 
        if (videoData->keyFrame)
        {
            if (videoData->channelNumber>CL_MAX_CHANNEL_NUMBER-1)
            {
                Q_ASSERT(false);
                return false;
            }

            m_gotKeyFrame[videoData->channelNumber]++;
        }
        else
        {
            // need key data but got not key data
            return false;
        }
    }

    data->dataProvider = this;

    if (videoData)
        m_stat[videoData->channelNumber].onData(data->data.size());

    return true;

}

