// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_archive_stream_reader.h"

#include <core/resource/media_resource.h>
#include <media/filters/abstract_media_data_filter.h>
#include <nx/media/video_data_packet.h>
#include <nx/utils/log/log.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

#include "abstract_archive_integrity_watcher.h"

QnAbstractArchiveStreamReader::QnAbstractArchiveStreamReader(const QnResourcePtr& resource):
    QnAbstractMediaStreamDataProvider(resource)
{
}

QnAbstractArchiveStreamReader::~QnAbstractArchiveStreamReader()
{
    stop();
}

QnAbstractNavigator *QnAbstractArchiveStreamReader::navDelegate() const
{
    return m_navDelegate;
}

// ------------------- Audio tracks -------------------------

unsigned QnAbstractArchiveStreamReader::getCurrentAudioChannel() const
{
    return 0;
}

CameraDiagnostics::Result QnAbstractArchiveStreamReader::diagnoseMediaStreamConnection()
{
    //TODO/IMPL
    return CameraDiagnostics::Result(CameraDiagnostics::ErrorCode::unknown );
}

QStringList QnAbstractArchiveStreamReader::getAudioTracksInfo() const
{
    return QStringList();
}

bool QnAbstractArchiveStreamReader::setAudioChannel(unsigned /*num*/)
{
    return false;
}

void QnAbstractArchiveStreamReader::setNavDelegate(QnAbstractNavigator* navDelegate)
{
    m_navDelegate = navDelegate;
}

void QnAbstractArchiveStreamReader::setCycleMode(bool value)
{
    m_cycleMode = value;
}

void QnAbstractArchiveStreamReader::jumpToPreviousFrame(qint64 usec)
{
    if (usec != DATETIME_NOW)
        jumpTo(qMax(0ll, usec - 200 * 1000), usec-1);
    else
        jumpTo(usec, 0);
}

void QnAbstractArchiveStreamReader::addMediaFilter(const std::shared_ptr<AbstractMediaDataFilter>& filter)
{
    m_filters.push_back(filter);
}

void QnAbstractArchiveStreamReader::resetRealtimeDelay()
{
    m_sendTimeBaseUs = -1;
}

std::chrono::milliseconds QnAbstractArchiveStreamReader::getDelay(int64_t timestamp)
{
    if (!m_realTimeSpeed.has_value())
        return std::chrono::milliseconds();

    if (m_sendTimeBaseUs == -1)
    {
        m_timer.restart();
        m_sendTimeBaseUs = timestamp;
        return std::chrono::milliseconds();
    }

    const int64_t dtUs = (timestamp - m_sendTimeBaseUs) / *m_realTimeSpeed;
    const auto sleepTime = std::chrono::milliseconds(dtUs / 1000) - m_timer.elapsed();
    if (sleepTime > MAX_FRAME_DURATION)
    {
        m_timer.restart();
        m_sendTimeBaseUs = timestamp;
        return std::chrono::milliseconds(); //< Ignore archive gaps.
    }
    if (sleepTime.count() > 0)
        return sleepTime;

    return std::chrono::milliseconds();
}

void QnAbstractArchiveStreamReader::run()
{
    initSystemThreadId();

    NX_VERBOSE(this, "Started");
    beforeRun();

    while(!needToStop())
    {
        pauseDelay(); // pause if needed;
        if (needToStop()) // extra check after pause
            break;

        // check queue sizes
        if (!dataCanBeAccepted())
        {
            emit waitForDataCanBeAccepted();
            QnSleep::msleep(10);
            continue;
        }

        QnAbstractMediaDataPtr data = getNextData();

        if (data && m_realTimeSpeed.has_value())
        {
            auto mediaRes = resource().dynamicCast<QnMediaResource>();
            if (mediaRes && !mediaRes->hasVideo(this) && data->dataType == QnAbstractMediaData::AUDIO)
                std::this_thread::sleep_for(getDelay(data->timestamp));
            else if (data->dataType == QnAbstractMediaData::VIDEO)
            {
                auto d = getDelay(data->timestamp);
                std::this_thread::sleep_for(d);
            }
        }

        if (data)
        {
            NX_VERBOSE(this, "New data, timestamp: %1, type: %2, flags: %3, channel: %4",
                data->timestamp, data->dataType, data->flags, data->channelNumber);
        }
        else
        {
            NX_VERBOSE(this, "Null data");
        }

        for (const auto& filter: m_filters)
        {
            data = std::const_pointer_cast<QnAbstractMediaData>(
                std::dynamic_pointer_cast<const QnAbstractMediaData>(filter->processData(data)));
        }


        if (m_noDataHandler && (!data || data->dataType == QnAbstractMediaData::EMPTY_DATA))
            m_noDataHandler();

        if (!data && !needToStop())
        {
            setNeedKeyData();
            onEvent(CameraDiagnostics::BadMediaStreamResult());

            QnSleep::msleep(30);
            continue;
        }

        checkAndFixTimeFromCamera(data);

        QnCompressedVideoDataPtr videoData = std::dynamic_pointer_cast<QnCompressedVideoData>(data);

        if (videoData && videoData->channelNumber>CL_MAX_CHANNEL_NUMBER-1)
        {
            NX_ASSERT(false);
            continue;
        }

        if (videoData && needKeyData(videoData->channelNumber))
        {
            if (videoData->flags & AV_PKT_FLAG_KEY)
            {
                m_gotKeyFrame.at(videoData->channelNumber)++;
            }
            else
            {
                NX_VERBOSE(this, "Skip non-key frame: %1, type: %2", data->timestamp, data->dataType);
                continue; // need key data but got not key data
            }
        }

        if(data)
        {
            data->dataProvider = this;
            if (data->flags &
                (QnAbstractMediaData::MediaFlags_AfterEOF | QnAbstractMediaData::MediaFlags_BOF))
            {
                m_stat[data->channelNumber].reset();
            }
        }

        auto mediaRes = resource().dynamicCast<QnMediaResource>();
        if (mediaRes && !mediaRes->hasVideo(this))
        {
            if (data)
                m_stat[data->channelNumber].onData(data);
        }
        else {
            if (videoData)
                m_stat[data->channelNumber].onData(data);
        }

        putData(std::move(data));
    }

    afterRun();
    NX_VERBOSE(this, "Stopped");
}

void QnAbstractArchiveStreamReader::setNoDataHandler(
    nx::MoveOnlyFunc<void()> noDataHandler)
{
    m_noDataHandler = std::move(noDataHandler);
}
