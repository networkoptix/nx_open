// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_time_helper.h"

#include <nx/streaming/nx_streaming_ini.h>
#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

namespace nx::streaming::rtp {

using namespace std::chrono;

namespace {

constexpr auto kMaxAdjustmentHistoryRecordAge = 30s;
constexpr auto kWaitSenderReportThreshold = 20s;

} // namespace

CameraTimeHelper::CameraTimeHelper(
    nx::rtp::Sdp::MediaType mediaType,
    const std::string& resourceId,
    const TimeOffsetPtr& offset)
    :
    m_primaryOffset(offset),
    m_resyncThreshold(milliseconds(nxStreamingIni().resyncThresholdMs)),
    m_streamsSyncThreshold(milliseconds(nxStreamingIni().streamsSyncThresholdMs)),
    m_forceCameraTimeThreshold(
        mediaType == nx::rtp::Sdp::MediaType::Metadata
            ? milliseconds(nxStreamingIni().cameraMetadataTimeThresholdMs)
            : milliseconds(nxStreamingIni().forceCameraTimeThresholdMs)),
    m_maxExpectedMetadataDelay(milliseconds(nxStreamingIni().maxExpectedMetadataDelayMs)),
    m_resourceId(resourceId),
    m_mediaType(mediaType),
    m_adjustmentHistory(kMaxAdjustmentHistoryRecordAge)
{
    m_waitSenderReportTimer.restart();
}

void CameraTimeHelper::setTimePolicy(TimePolicy policy)
{
    m_timePolicy = policy;
}

void CameraTimeHelper::resetBadCameraTimeState()
{
    m_badCameraTimeState = false;
}

void CameraTimeHelper::reset()
{
    m_localOffset.initialized = false;
    m_adjustmentHistory.reset();
}

void CameraTimeHelper::stopWaitingSenderReport()
{
    m_waitSenderReportTimer.invalidate();
}

std::chrono::microseconds CameraTimeHelper::replayAdjustmentFromHistory(
    std::chrono::microseconds cameraTimestamp)
{
    const auto latestHistoryTimestamp = m_adjustmentHistory.getLatestSourceTimestamp();
    if (latestHistoryTimestamp
        && *latestHistoryTimestamp - kMaxAdjustmentHistoryRecordAge <= cameraTimestamp
        && cameraTimestamp <= *latestHistoryTimestamp + m_forceCameraTimeThreshold)
    {
        return m_adjustmentHistory.replay(cameraTimestamp);
    }

    const auto vmsSystemTimestamp = qnSyncTime->currentTimePoint();
    const auto lowBound =
        vmsSystemTimestamp - m_forceCameraTimeThreshold - m_maxExpectedMetadataDelay;
    const auto highBound =
        vmsSystemTimestamp + m_forceCameraTimeThreshold;
    if (lowBound < cameraTimestamp && cameraTimestamp < highBound)
        return cameraTimestamp;

    NX_MUTEX_LOCKER locker(&m_historicalAdjustmentDeltaMutex);
    if (!m_historicalAdjustmentDelta)
        m_historicalAdjustmentDelta = vmsSystemTimestamp - cameraTimestamp;

    return cameraTimestamp + *m_historicalAdjustmentDelta;
}

microseconds CameraTimeHelper::getTime(
    const microseconds currentTime,
    const microseconds cameraTime,
    const bool isUtcTime,
    const bool isPrimaryStream,
    EventCallback callback)
{
    bool isCameraTimeAccurate = abs(cameraTime - currentTime) < m_forceCameraTimeThreshold;
    if (m_timePolicy == TimePolicy::forceCameraTime)
    {
        if (isPrimaryStream)
            m_adjustmentHistory.record(cameraTime, 0us);
        return cameraTime;
    }

    if (m_timePolicy == TimePolicy::useCameraTimeIfCorrect)
    {
        if (isUtcTime && isCameraTimeAccurate)
        {
            if (m_badCameraTimeState)
            {
                NX_DEBUG(this, "Camera time is back to normal, camera time will used");
                callback(EventType::CameraTimeBackToNormal);
            }
            m_badCameraTimeState = false;
            if (isPrimaryStream)
                m_adjustmentHistory.record(cameraTime, 0us);
            NX_VERBOSE(this,
                "ResourceId: %1, camera time is accurate: %2ms, system time: %3ms, dt: %4ms",
                getName(),
                cameraTime.count() / 1000,
                currentTime.count() / 1000,
                (cameraTime - currentTime).count() / 1000);
            return cameraTime;
        }
        else if (!m_badCameraTimeState && m_waitSenderReportTimer.hasExpired(kWaitSenderReportThreshold))
        {
            NX_DEBUG(this,
                "ResourceId: %1, camera time is not accurate: %2ms, system time: %3ms, dt: %4ms"
                ", system time will used",
                getName(), cameraTime.count() / 1000, currentTime.count() / 1000,
                (cameraTime - currentTime).count() / 1000);
            if (isPrimaryStream)
            {
                callback(EventType::BadCameraTime);
                m_badCameraTimeState = true;
            }
        }
    }

    bool hasPrimaryOffset = m_primaryOffset && m_primaryOffset->initialized;
    bool isStreamDesync = !isPrimaryStream && hasPrimaryOffset &&
        abs(m_primaryOffset->value.load() + cameraTime - currentTime) > m_streamsSyncThreshold;

    if (isStreamDesync)
        callback(EventType::StreamDesync);

    bool useLocalOffset =
        !isUtcTime || // no UTC time -> can not sync streams
        isStreamDesync || // streams desync exceeds the threshold
        !m_primaryOffset || // primary offset not used
        (!hasPrimaryOffset && !isPrimaryStream); // primary stream not init offset yet

    TimeOffset& offset = useLocalOffset ? m_localOffset : *m_primaryOffset;
    if (!offset.initialized)
    {
        offset.initialized = true;
        offset.value = currentTime - cameraTime;
    }

    const microseconds deviation = abs(offset.value.load() + cameraTime - currentTime);

    NX_VERBOSE(this, "Camera time: %1, vms time: %2, dt: %3ms, deviation: %4, offset: %5, isPrimaryStream: %6, useLocalOffset: %7, resyncThreshold: %8",
        std::chrono::duration_cast<std::chrono::milliseconds>(cameraTime),
        std::chrono::duration_cast<std::chrono::milliseconds>(currentTime),
        (cameraTime - currentTime).count() / 1000,
        std::chrono::duration_cast<std::chrono::milliseconds>(deviation),
        std::chrono::duration_cast<std::chrono::milliseconds>(offset.value.load()),
        isPrimaryStream,
        useLocalOffset,
        m_resyncThreshold);

    if ((isPrimaryStream || useLocalOffset) && deviation >= m_resyncThreshold)
    {
        NX_DEBUG(this,
            "ResourceId: %1. Resync camera time, deviation %2ms, cameraTime: %3ms, isPrimaryStream %4, useLocalOffset %5",
            getName(),
            deviation.count() / 1000,
            cameraTime.count() / 1000,
            isPrimaryStream,
            useLocalOffset);
        offset.value = currentTime - cameraTime;
        if (isPrimaryStream)
            callback(EventType::ResyncToLocalTime);
    }
    const auto offsetValue = offset.value.load();
    if (isPrimaryStream)
        m_adjustmentHistory.record(cameraTime, offsetValue);
    return offsetValue + cameraTime;
}

std::string CameraTimeHelper::getName() const
{
    return nx::format("%1(%2)", m_resourceId, toString(m_mediaType)).toStdString();
}

} //nx::streaming::rtp
