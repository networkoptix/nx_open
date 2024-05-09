// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_media_stream_data_provider.h"

#include <chrono>

#include <core/resource/camera_resource.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/config.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/nx_streaming_ini.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time_helper.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

using namespace std::chrono_literals;

static const qint64 TIME_RESYNC_THRESHOLD = 15'000'000ll;
static const qint64 kMinFrameDurationUsec = 1'000;
static const int kMaxErrorsToReportIssue = 2;

static std::chrono::microseconds sMediaStatisticsWindowSize = 2s;
static int sMediaStatisticsMaxDurationInFrames = 0;

void QnAbstractMediaStreamDataProvider::setMediaStatisticsWindowSize(
    std::chrono::microseconds value)
{
    sMediaStatisticsWindowSize = value;
}

void QnAbstractMediaStreamDataProvider::setMediaStatisticsMaxDurationInFrames(int value)
{
    sMediaStatisticsMaxDurationInFrames = value;
}

bool QnAbstractMediaStreamDataProvider::isConnectionLost() const
{
    return m_numberOfErrors >= kMaxErrorsToReportIssue;
}

void QnAbstractMediaStreamDataProvider::onEvent(CameraDiagnostics::Result event)
{
    if (event.errorCode == CameraDiagnostics::ErrorCode::noError)
        m_numberOfErrors = 0;
    else
        ++m_numberOfErrors;
}

QnAbstractMediaStreamDataProvider::QnAbstractMediaStreamDataProvider(
    const QnResourcePtr& resource)
    :
    QnAbstractStreamDataProvider(resource),
    m_numberOfChannels(
        [this]
        {
            return m_mediaResource ? m_mediaResource->getVideoLayout(this)->channelCount() : 1;
        })
{
    std::fill(m_gotKeyFrame.begin(), m_gotKeyFrame.end(), 0);
    m_mediaResource = resource.dynamicCast<QnMediaResource>();
    NX_ASSERT(m_mediaResource);
    resetTimeCheck();
    m_isCamera = resource.dynamicCast<QnSecurityCamResource>();
    connect(resource.data(), &QnResource::propertyChanged, this,
        [this](const QnResourcePtr& /*resource*/, const QString& propertyName)
        {
            if (propertyName == ResourcePropertyKey::kVideoLayout)
                m_numberOfChannels.reset();
        });
}

QnAbstractMediaStreamDataProvider::~QnAbstractMediaStreamDataProvider()
{
    stop();
}

void QnAbstractMediaStreamDataProvider::setNeedKeyData(int channel)
{
    if (channel < CL_MAX_CHANNEL_NUMBER)
        m_gotKeyFrame.at(channel) = 0;
}

void QnAbstractMediaStreamDataProvider::setNeedKeyData()
{
    for (int i = 0; i < getNumberOfChannels(); ++i)
        m_gotKeyFrame.at(i) = 0;
}

bool QnAbstractMediaStreamDataProvider::needKeyData(int channel) const
{
    if (channel < CL_MAX_CHANNEL_NUMBER)
        return m_gotKeyFrame.at(channel) == 0;
    return false;
}

bool QnAbstractMediaStreamDataProvider::needKeyData() const
{
    for (int i = 0; i < getNumberOfChannels(); ++i)
    {
        if (m_gotKeyFrame.at(i) == 0)
            return true;
    }

    return false;
}

void QnAbstractMediaStreamDataProvider::beforeRun()
{
    m_numberOfChannels.reset();
    setNeedKeyData();
    resetMediaStatistics();
    m_numberOfErrors = 0;
}

void QnAbstractMediaStreamDataProvider::resetMediaStatistics()
{
    for (int i = 0; i < CL_MAX_CHANNEL_NUMBER; ++i)
    {
        m_stat[i].setWindowSize(sMediaStatisticsWindowSize);
        m_stat[i].setMaxDurationInFrames(sMediaStatisticsMaxDurationInFrames);
        m_stat[i].reset();
    }
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
    NX_ASSERT(m_numberOfChannels.get(), "No channels?");
    return std::min(CL_MAX_CHANNEL_NUMBER, std::max(1, m_numberOfChannels.get()));
}

qint64 QnAbstractMediaStreamDataProvider::bitrateBitsPerSecond() const
{
    qint64 rez = 0;
    for (int i = 0; i < getNumberOfChannels(); ++i)
        rez += m_stat[i].bitrateBitsPerSecond();
    return rez;
}

float QnAbstractMediaStreamDataProvider::getFrameRate() const
{
    float rez = 0;
    for (int i = 0; i < getNumberOfChannels(); ++i)
        rez += m_stat[i].getFrameRate();

    return rez / getNumberOfChannels();
}

float QnAbstractMediaStreamDataProvider::getAverageGopSize() const
{
    float rez = 0;
    for (int i = 0; i < getNumberOfChannels(); ++i)
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
                qint64 timeDiff = media->timestamp - m_lastMediaTime[channel];
                // if timeDiff < -N it may be time correction or daylight time change
                if (timeDiff >= -TIME_RESYNC_THRESHOLD && timeDiff < kMinFrameDurationUsec)
                {
                    // Most likely, timestamps reported by the camera are not so good.
                    NX_DEBUG(this, "Timestamp correction. ts diff %1, camera %2, %3 stream",
                        timeDiff,
                        m_resource,
                        media->isLQ() ? "low" : "high");

                    media->timestamp = m_lastMediaTime[channel] + kMinFrameDurationUsec;
                }
            }
        }
        m_lastMediaTime[channel] = media->timestamp;
    }
}

CodecParametersConstPtr QnAbstractMediaStreamDataProvider::getCodecContext() const
{
    return CodecParametersConstPtr(nullptr);
}

CameraDiagnostics::Result QnAbstractMediaStreamDataProvider::diagnoseMediaStreamConnection()
{
    return CameraDiagnostics::NotImplementedResult();
}
