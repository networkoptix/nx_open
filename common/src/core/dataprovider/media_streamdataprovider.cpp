#include "media_streamdataprovider.h"
#include "core/resource/resource_media_layout.h"
#include "core/datapacket/mediadatapacket.h"
#include "utils/common/sleep.h"
#include "utils/common/util.h"
#include "../resource/camera_resource.h"

QnAbstractMediaStreamDataProvider::QnAbstractMediaStreamDataProvider(QnResourcePtr res):
QnAbstractStreamDataProvider(res),
m_numberOfchannels(0)
{
    memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
    m_mediaResource = qSharedPointerDynamicCast<QnMediaResource>(res);
    Q_ASSERT(m_mediaResource);
    memset(m_lastVideoTime, 0, sizeof(m_lastVideoTime));

    //QnMediaResourcePtr mr = getResource().dynamicCast<QnMediaResource>();
    //m_NumaberOfVideoChannels = mr->getMediaLayout()->numberOfVideoChannels();
}

QnAbstractMediaStreamDataProvider::~QnAbstractMediaStreamDataProvider()
{
    stop();
}




void QnAbstractMediaStreamDataProvider::setNeedKeyData()
{
    QMutexLocker mtx(&m_mutex);

    if (m_numberOfchannels==0)
        m_numberOfchannels = m_mediaResource->getVideoLayout(this)->numberOfChannels();

    
    for (int i = 0; i < m_numberOfchannels; ++i)
        m_gotKeyFrame[i] = 0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData(int channel) const
{
    QMutexLocker mtx(&m_mutex);
    return m_gotKeyFrame[channel]==0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData() const
{
    QMutexLocker mtx(&m_mutex);

    if (m_numberOfchannels==0)
        m_numberOfchannels = m_mediaResource->getVideoLayout(this)->numberOfChannels();

   
    for (int i = 0; i < m_numberOfchannels; ++i)
        if (m_gotKeyFrame[i]==0)
            return true;

    return false;
}


void QnAbstractMediaStreamDataProvider::beforeRun()
{
    setNeedKeyData();
    mFramesLost = 0;
}

void QnAbstractMediaStreamDataProvider::afterRun()
{
    for (int i = 0; i < CL_MAX_CHANNEL_NUMBER; ++i)
    {
        m_stat[i].resetStatistics();
    }
}


bool QnAbstractMediaStreamDataProvider::afterGetData(QnAbstractDataPacketPtr d)
{

    QnAbstractMediaDataPtr data = qSharedPointerDynamicCast<QnAbstractMediaData>(d);

    if (data==0)
    {
        setNeedKeyData();
        ++mFramesLost;
        m_stat[0].onData(0);
        m_stat[0].onEvent(CL_STAT_FRAME_LOST);

        if (mFramesLost == MAX_LOST_FRAME) // if we lost 2 frames => connection is lost for sure (2)
            m_stat[0].onLostConnection();

        QnSleep::msleep(10);

        return false;
    }

    QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(data);

    if (mFramesLost > 0) // we are alive again
    {
        if (mFramesLost >= MAX_LOST_FRAME)
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

    if(data)
        data->dataProvider = this;

    if (videoData)
        m_stat[videoData->channelNumber].onData(data->data.size());

    return true;


}

const QnStatistics* QnAbstractMediaStreamDataProvider::getStatistics(int channel) const
{
    return &m_stat[channel];
}

float QnAbstractMediaStreamDataProvider::getBitrate() const
{
    float rez = 0;
    for (int i = 0; i < m_numberOfchannels; ++i)
        rez += m_stat[i].getBitrate();
    return rez;
}

void QnAbstractMediaStreamDataProvider::checkTime(QnAbstractMediaDataPtr data)
{
    QnPhysicalCameraResourcePtr camera = qSharedPointerDynamicCast<QnPhysicalCameraResource> (getResource());
    if (camera)
    {
        // correct packets timestamp if we have got several packets very fast
        QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(data);
        if (video)
        {
            qint64 timeDiff = video->timestamp - m_lastVideoTime[video->channelNumber];
            // if timeDiff < -N it may be time correction or dayling time change
            if (timeDiff >= -1000ll*1000 && timeDiff < MIN_FRAME_DURATION*1000)
            {
                video->timestamp = m_lastVideoTime[video->channelNumber] + MIN_FRAME_DURATION*1000ll;
            }
            m_lastVideoTime[video->channelNumber] = video->timestamp;
        }
    }
}

QnMediaContextPtr QnAbstractMediaStreamDataProvider::getCodecContext() const
{
    return QnMediaContextPtr();
}
