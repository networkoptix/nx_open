#include "media_streamdataprovider.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "core/resource/resource_media_layout.h"
#include "core/datapacket/media_data_packet.h"
#include "core/datapacket/video_data_packet.h"
#include "utils/common/sleep.h"
#include "utils/common/util.h"
#include "../resource/camera_resource.h"

static const qint64 TIME_RESYNC_THRESHOLD = 1000000ll * 15;

QnAbstractMediaStreamDataProvider::QnAbstractMediaStreamDataProvider(const QnResourcePtr& res)
:
    QnAbstractStreamDataProvider(res),
    m_numberOfchannels(0)
{
    memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
    m_mediaResource = res;
    Q_ASSERT(dynamic_cast<QnMediaResource*>(m_mediaResource.data()));
    resetTimeCheck();
    m_isCamera = dynamic_cast<const QnPhysicalCameraResource*>(res.data()) != nullptr;
    //QnMediaResourcePtr mr = getResource().dynamicCast<QnMediaResource>();
    //m_NumaberOfVideoChannels = mr->getMediaLayout()->numberOfVideoChannels();
}

QnAbstractMediaStreamDataProvider::~QnAbstractMediaStreamDataProvider()
{
    stop();
}




void QnAbstractMediaStreamDataProvider::setNeedKeyData()
{
    SCOPED_MUTEX_LOCK( mtx, &m_mutex);

    if (m_numberOfchannels==0)
        m_numberOfchannels = dynamic_cast<QnMediaResource*>(m_mediaResource.data())->getVideoLayout(this)->channelCount();

    
    for (int i = 0; i < m_numberOfchannels; ++i)
        m_gotKeyFrame[i] = 0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData(int channel) const
{
    SCOPED_MUTEX_LOCK( mtx, &m_mutex);
    return m_gotKeyFrame[channel]==0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData() const
{
    SCOPED_MUTEX_LOCK( mtx, &m_mutex);

    if (m_numberOfchannels==0)
        m_numberOfchannels = dynamic_cast<QnMediaResource*>(m_mediaResource.data())->getVideoLayout(this)->channelCount();

   
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


bool QnAbstractMediaStreamDataProvider::afterGetData(const QnAbstractDataPacketPtr& d)
{

    QnAbstractMediaData* data = dynamic_cast<QnAbstractMediaData*>(d.data());

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

    const QnCompressedVideoData* videoData = dynamic_cast<const QnCompressedVideoData*>(data);

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
        m_stat[videoData->channelNumber].onData(data->dataSize());

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

void QnAbstractMediaStreamDataProvider::resetTimeCheck()
{
    for (int i = 0; i < CL_MAX_CHANNELS+1; ++i)
        m_lastMediaTime[i] = AV_NOPTS_VALUE;
}

void QnAbstractMediaStreamDataProvider::checkTime(const QnAbstractMediaDataPtr& media)
{
    if (m_isCamera && media && (media->dataType == QnAbstractMediaData::VIDEO || media->dataType == QnAbstractMediaData::AUDIO))
    {
        // correct packets timestamp if we have got several packets very fast

        if (media->flags & (QnAbstractMediaData::MediaFlags_BOF | QnAbstractMediaData::MediaFlags_ReverseBlockStart)) {
            resetTimeCheck();
        }
        else if ((quint64)m_lastMediaTime[media->channelNumber] != AV_NOPTS_VALUE) {
            qint64 timeDiff = media->timestamp - m_lastMediaTime[media->channelNumber];
            // if timeDiff < -N it may be time correction or dayling time change
            if (timeDiff >=-TIME_RESYNC_THRESHOLD  && timeDiff < MIN_FRAME_DURATION)
            {
                media->timestamp = m_lastMediaTime[media->channelNumber] + MIN_FRAME_DURATION;
            }
        }
        m_lastMediaTime[media->channelNumber] = media->timestamp;
    }
}

QnMediaContextPtr QnAbstractMediaStreamDataProvider::getCodecContext() const
{
    return QnMediaContextPtr();
}

CameraDiagnostics::Result QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection()
{
    return CameraDiagnostics::NotImplementedResult();
}

#endif // ENABLE_DATA_PROVIDERS
