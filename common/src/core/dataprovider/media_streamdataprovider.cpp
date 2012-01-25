#include "media_streamdataprovider.h"
#include "core/resource/resource_media_layout.h"
#include "core/datapacket/mediadatapacket.h"
#include "utils/common/sleep.h"
#include "core/resource/resource_media_layout.h"


QnAbstractMediaStreamDataProvider::QnAbstractMediaStreamDataProvider(QnResourcePtr res):
QnAbstractStreamDataProvider(res),
m_quality(QnQualityNormal),
m_fps(MAX_LIVE_FPS),
m_numberOfchannels(0)
{
    memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
    m_mediaResource = qSharedPointerDynamicCast<QnMediaResource>(res);
    Q_ASSERT(m_mediaResource);
    m_channel_number = 1;

    //QnMediaResourcePtr mr = getResource().dynamicCast<QnMediaResource>();
    //m_NumaberOfVideoChannels = mr->getMediaLayout()->numberOfVideoChannels();
}

QnAbstractMediaStreamDataProvider::~QnAbstractMediaStreamDataProvider()
{
    stop();
}



void QnAbstractMediaStreamDataProvider::setQuality(QnStreamQuality q)
{
    QMutexLocker mtx(&m_mutex);
    m_quality = q;
    updateStreamParamsBasedOnQuality();
    //setNeedKeyData();
}

QnStreamQuality QnAbstractMediaStreamDataProvider::getQuality() const
{
    QMutexLocker mtx(&m_mutex);
    return m_quality;
}

// for live providers only
void QnAbstractMediaStreamDataProvider::setFps(float f)
{
    QMutexLocker mtx(&m_mutex);
    m_fps = f;
    updateStreamParamsBasedOnFps();
}

float QnAbstractMediaStreamDataProvider::getFps() const
{
    QMutexLocker mtx(&m_mutex);
    return m_fps;
}

bool QnAbstractMediaStreamDataProvider::isMaxFps() const
{
    QMutexLocker mtx(&m_mutex);
    return abs( m_fps - MAX_LIVE_FPS)< .1;
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

    m_framesSinceLastMetaData = 0;
    m_timeSinceLastMetaData.restart();

}

void QnAbstractMediaStreamDataProvider::afterRun()
{

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

        if (mFramesLost == 4) // if we lost 2 frames => connection is lost for sure (2)
            m_stat[0].onLostConnection();

        QnSleep::msleep(10);

        return false;
    }

    QnCompressedVideoDataPtr videoData = qSharedPointerDynamicCast<QnCompressedVideoData>(data);

    if (mFramesLost > 0) // we are alive again
    {
        if (mFramesLost >= 4)
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

    data->dataProvider = this;

    if (videoData)
        m_stat[videoData->channelNumber].onData(data->data.size());

    return true;

}

const QnStatistics* QnAbstractMediaStreamDataProvider::getStatistics(int channel) const
{
    return &m_stat[channel];
}

bool QnAbstractMediaStreamDataProvider::needMetaData() const
{
    return (m_framesSinceLastMetaData > 10 || m_timeSinceLastMetaData.elapsed() > META_DATA_DURATION_MS) &&
        m_framesSinceLastMetaData > 0; // got at least one frame
}
