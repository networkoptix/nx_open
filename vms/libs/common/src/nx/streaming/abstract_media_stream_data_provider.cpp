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
static const qint64 kMinAudioFrameDurationUsec = 1000;
static const int kMaxErrorsToReportIssue = 2;

bool QnAbstractMediaStreamDataProvider::isConnectionLost() const
{
    return m_numberOfErrors >= kMaxErrorsToReportIssue;
}

void QnAbstractMediaStreamDataProvider::onEvent(
    std::chrono::microseconds /*timestamp*/,
    CameraDiagnostics::Result event)
{
    if (event.errorCode == CameraDiagnostics::ErrorCode::noError)
    {
        if (m_numberOfErrors > 0)
        {
            m_numberOfErrors = 0;
            emit streamEvent(this, event); //< Report no error.
        }
    }
    else
    {
        ++m_numberOfErrors;
        if (m_numberOfErrors % kMaxErrorsToReportIssue == 0)
            emit streamEvent(this, event);
    }
}

QnAbstractMediaStreamDataProvider::QnAbstractMediaStreamDataProvider(const QnResourcePtr& res)
    :
    QnAbstractStreamDataProvider(res),
    m_numberOfchannels(1)
{
    memset(m_gotKeyFrame, 0, sizeof(m_gotKeyFrame));
    m_mediaResource = res;
    NX_ASSERT(dynamic_cast<QnMediaResource*>(m_mediaResource.data()));
    resetTimeCheck();
    m_isCamera = dynamic_cast<const QnSecurityCamResource*>(res.data()) != nullptr;
}

QnAbstractMediaStreamDataProvider::~QnAbstractMediaStreamDataProvider()
{
    stop();
}

void QnAbstractMediaStreamDataProvider::setNeedKeyData(int channel)
{
    QnMutexLocker mtx( &m_mutex );

    loadNumberOfChannelsIfUndetected();

    if (m_numberOfchannels < CL_MAX_CHANNEL_NUMBER && channel < m_numberOfchannels)
        m_gotKeyFrame[channel] = 0;
}

void QnAbstractMediaStreamDataProvider::loadNumberOfChannelsIfUndetected() const
{
    if (m_numberOfchannels != 0)
        return;
    if (auto mediaRes = dynamic_cast<QnMediaResource*>(m_mediaResource.data()))
        m_numberOfchannels = mediaRes->getVideoLayout(this)->channelCount();
}

void QnAbstractMediaStreamDataProvider::setNeedKeyData()
{
    QnMutexLocker mtx( &m_mutex );

    loadNumberOfChannelsIfUndetected();

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

    loadNumberOfChannelsIfUndetected();

    for (int i = 0; i < m_numberOfchannels; ++i)
        if (m_gotKeyFrame[i]==0)
            return true;

    return false;
}

void QnAbstractMediaStreamDataProvider::beforeRun()
{
    setNeedKeyData();
    for (int i = 0; i < CL_MAX_CHANNEL_NUMBER; ++i)
        m_stat[i].reset();
    m_numberOfErrors = 0;
}

void QnAbstractMediaStreamDataProvider::afterRun()
{
}

const QnMediaStreamStatistics* QnAbstractMediaStreamDataProvider::getStatistics(int channel) const
{
    return &m_stat[channel];
}

int QnAbstractMediaStreamDataProvider::getNumberOfChannels() const
{
    NX_ASSERT(m_numberOfchannels, "No channels?");
    return m_numberOfchannels ? m_numberOfchannels : 1;
}

qint64 QnAbstractMediaStreamDataProvider::bitrateBitsPerSecond() const
{
    float rez = 0;
    for (int i = 0; i < m_numberOfchannels; ++i)
        rez += m_stat[i].bitrateBitsPerSecond();
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

void QnAbstractMediaStreamDataProvider::checkAndFixTimeFromCamera(const QnAbstractMediaDataPtr& media)
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
                int64_t minFrameDurationUsec = media->dataType == QnAbstractMediaData::AUDIO ?
                    kMinAudioFrameDurationUsec : MIN_FRAME_DURATION_USEC;

                qint64 timeDiff = media->timestamp - m_lastMediaTime[channel];
                // if timeDiff < -N it may be time correction or dayling time change
                if (timeDiff >= -TIME_RESYNC_THRESHOLD && timeDiff < minFrameDurationUsec)
                {
                    // Most likely, timestamps reported by the camera are not so good.
                    NX_VERBOSE(this, lit("Timestamp correction. ts diff %1, camera %2, %3 stream").
                        arg(timeDiff).
                        arg(m_mediaResource ? m_mediaResource->getName() : QString()).
                        arg((media->flags & QnAbstractMediaData::MediaFlags_LowQuality) ? lit("low") : lit("high")));

                    media->timestamp = m_lastMediaTime[channel] + minFrameDurationUsec;
                }
            }
        }
        m_lastMediaTime[channel] = media->timestamp;
    }
}

QnConstMediaContextPtr QnAbstractMediaStreamDataProvider::getCodecContext() const
{
    return QnConstMediaContextPtr(nullptr);
}

CameraDiagnostics::Result QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection()
{
    return CameraDiagnostics::NotImplementedResult();
}
