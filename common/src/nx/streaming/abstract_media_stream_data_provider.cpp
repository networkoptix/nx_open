#include "abstract_media_stream_data_provider.h"

#include <core/resource/resource_media_layout.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/config.h>
#include <utils/common/sleep.h>
#include <utils/common/util.h>
#include <nx/utils/log/log.h>
#include <core/resource/camera_resource.h>
#include <nx/streaming/nx_streaming_ini.h>
#include <utils/common/synctime.h>
#include <nx/utils/time_helper.h>

static const qint64 TIME_RESYNC_THRESHOLD = 1000000ll * 15;

QnAbstractMediaStreamDataProvider::QnAbstractMediaStreamDataProvider(const QnResourcePtr& res)
:
    QnAbstractStreamDataProvider(res),
    m_numberOfchannels(0)
{
    memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
    m_mediaResource = res;
    NX_ASSERT(dynamic_cast<QnMediaResource*>(m_mediaResource.data()));
    resetTimeCheck();
    m_isCamera = dynamic_cast<const QnSecurityCamResource*>(res.data()) != nullptr;
    //QnMediaResourcePtr mr = getResource().dynamicCast<QnMediaResource>();
    //m_NumaberOfVideoChannels = mr->getMediaLayout()->numberOfVideoChannels();
}

QnAbstractMediaStreamDataProvider::~QnAbstractMediaStreamDataProvider()
{
    stop();
}

void QnAbstractMediaStreamDataProvider::setNeedKeyData(int channel)
{
    QnMutexLocker mtx( &m_mutex );

    if (m_numberOfchannels == 0)
    {
        m_numberOfchannels = dynamic_cast<QnMediaResource*>(
            m_mediaResource.data())->getVideoLayout(this)->channelCount();
    }

    if (m_numberOfchannels < CL_MAX_CHANNEL_NUMBER && channel < m_numberOfchannels)
        m_gotKeyFrame[channel] = 0;
}

void QnAbstractMediaStreamDataProvider::setNeedKeyData()
{
    QnMutexLocker mtx( &m_mutex );

    if (m_numberOfchannels==0)
        m_numberOfchannels = dynamic_cast<QnMediaResource*>(m_mediaResource.data())->getVideoLayout(this)->channelCount();


    for (int i = 0; i < m_numberOfchannels; ++i)
        m_gotKeyFrame[i] = 0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData(int channel) const
{
    QnMutexLocker mtx( &m_mutex );
    return m_gotKeyFrame[channel]==0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData() const
{
    QnMutexLocker mtx( &m_mutex );

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
    m_framesLost = 0;
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

    QnAbstractMediaData* data = dynamic_cast<QnAbstractMediaData*>(d.get());

    if (data==0)
    {
        setNeedKeyData();
        ++m_framesLost;
        m_stat[0].onData(0, false);
        m_stat[0].onEvent(CL_STAT_FRAME_LOST);

        if (m_framesLost == MAX_LOST_FRAME) // if we lost 2 frames => connection is lost for sure (2)
            m_stat[0].onLostConnection();

        QnSleep::msleep(10);

        return false;
    }

    const QnCompressedVideoData* videoData = dynamic_cast<const QnCompressedVideoData*>(data);

    if (m_framesLost > 0) // we are alive again
    {
        if (m_framesLost >= MAX_LOST_FRAME)
        {
            m_stat[0].onEvent(CL_STAT_CAMRESETED);
        }

        m_framesLost = 0;
    }

    if (videoData && needKeyData())
    {
        // I do not like; need to do smth with it
        if (videoData->flags & AV_PKT_FLAG_KEY)
        {
            if (videoData->channelNumber>CL_MAX_CHANNEL_NUMBER-1)
            {
                NX_ASSERT(false);
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
        m_stat[videoData->channelNumber].onData(
            static_cast<unsigned int>(data->dataSize()),
            videoData->flags & AV_PKT_FLAG_KEY);

    return true;


}

const QnMediaStreamStatistics* QnAbstractMediaStreamDataProvider::getStatistics(int channel) const
{
    return &m_stat[channel];
}

int QnAbstractMediaStreamDataProvider::getNumberOfChannels() const
{
    NX_ASSERT(m_numberOfchannels, Q_FUNC_INFO, "No channels?");
    return m_numberOfchannels ? m_numberOfchannels : 1;
}

float QnAbstractMediaStreamDataProvider::getBitrateMbps() const
{
    float rez = 0;
    for (int i = 0; i < m_numberOfchannels; ++i)
        rez += m_stat[i].getBitrateMbps();
    return rez;
}

float QnAbstractMediaStreamDataProvider::getFrameRate() const
{
    float rez = 0;
    for (int i = 0; i < m_numberOfchannels; ++i)
        rez += m_stat[i].getFrameRate();

    return rez / getNumberOfChannels();
}

float QnAbstractMediaStreamDataProvider::getAverageGopSize() const
{
    float rez = 0;
    for (int i = 0; i < m_numberOfchannels; ++i)
        rez += m_stat[i].getAverageGopSize();

    return rez / getNumberOfChannels();
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
        int channel = media->channelNumber;
        if (media->dataType == QnAbstractMediaData::AUDIO)
            channel = CL_MAX_CHANNELS; //< use last vector element for audio timings control

        if (nxStreamingIni().enableTimeCorrection)
        {
            if (media->flags & (QnAbstractMediaData::MediaFlags_BOF | QnAbstractMediaData::MediaFlags_ReverseBlockStart))
            {
                resetTimeCheck();
            }
            else if (m_lastMediaTime[channel] != AV_NOPTS_VALUE)
            {
                qint64 timeDiff = media->timestamp - m_lastMediaTime[channel];
                // if timeDiff < -N it may be time correction or dayling time change
                if (timeDiff >= -TIME_RESYNC_THRESHOLD && timeDiff < MIN_FRAME_DURATION_USEC)
                {
                    // Most likely, timestamps reported by the camera are not so good.
                    NX_VERBOSE(this, lit("Timestamp correction. ts diff %1, camera %2, %3 stream").
                        arg(timeDiff).
                        arg(m_mediaResource ? m_mediaResource->getName() : QString()).
                        arg((media->flags & QnAbstractMediaData::MediaFlags_LowQuality) ? lit("low") : lit("high")));

                    media->timestamp = m_lastMediaTime[channel] + MIN_FRAME_DURATION_USEC;

                }
            }
        }
        m_lastMediaTime[channel] = media->timestamp;
    }
}

void QnAbstractMediaStreamDataProvider::checkAndFixTimeFromCamera(
    const QnAbstractMediaDataPtr& media)
{
    const int modulusUs = nxStreamingIni().unloopCameraPtsWithModulus;
    if (modulusUs <= 0)
    {
        checkTime(media);
        return;
    }

    if (!m_isCamera || !media || media->dataType != QnAbstractMediaData::VIDEO)
        return;

    const int channel = media->channelNumber;
    const qint64 pts = media->timestamp;
    media->timestamp = nx::utils::TimeHelper::unloopCameraPtsWithModulus(
        []() { return std::chrono::milliseconds(qnSyncTime->currentMSecsSinceEpoch()); },
        AV_NOPTS_VALUE,
        modulusUs,
        pts,
        m_lastMediaTime[channel],
        &m_unloopingPeriodStartUs);
    m_lastMediaTime[channel] = pts;
}

QnConstMediaContextPtr QnAbstractMediaStreamDataProvider::getCodecContext() const
{
    return QnConstMediaContextPtr(nullptr);
}

CameraDiagnostics::Result QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection()
{
    return CameraDiagnostics::NotImplementedResult();
}
