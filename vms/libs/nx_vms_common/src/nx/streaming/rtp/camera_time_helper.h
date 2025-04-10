// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <nx/rtp/sdp.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/timestamp_adjustment_history.h>

namespace nx::streaming::rtp {

enum class TimePolicy
{
    bindCameraTimeToLocalTime, //< Use camera time, bound to the server time.
    forceCameraTime, //< Use camera time only.
    useCameraTimeIfCorrect, //< Use camera time if it's close to the server time.
};

struct TimeOffset
{
    std::atomic<bool> initialized{false};
    std::atomic<std::chrono::microseconds> value{std::chrono::microseconds(0)};
};

using TimeOffsetPtr = std::shared_ptr<TimeOffset>;

class NX_VMS_COMMON_API CameraTimeHelper
{
public:
    enum class EventType
    {
        BadCameraTime,
        CameraTimeBackToNormal,
        StreamDesync,
        ResyncToLocalTime,
    };

    using EventCallback = std::function<void(EventType)>;

public:
    CameraTimeHelper(
        nx::rtp::Sdp::MediaType mediaType,
        const std::string& resourceId,
        const TimeOffsetPtr& offset);

    std::chrono::microseconds getTime(
        const std::chrono::microseconds currentTime,
        const std::chrono::microseconds cameraTime,
        const bool isUtcTime,
        const bool isPrimaryStream,
        EventCallback callback = [](CameraTimeHelper::EventType /*event*/){});

    void setResyncThreshold(std::chrono::milliseconds value) { m_resyncThreshold = value; }
    void setTimePolicy(TimePolicy policy);
    void resetBadCameraTimeState();
    void reset();
    void stopWaitingSenderReport();

    std::chrono::microseconds replayAdjustmentFromHistory(
        std::chrono::microseconds cameraTimestamp);

private:
    std::string getName() const;

private:
    TimeOffsetPtr m_primaryOffset;
    TimeOffset m_localOffset; //< used in case when no rtcp reports
    std::chrono::milliseconds m_resyncThreshold;
    const std::chrono::milliseconds m_streamsSyncThreshold;
    const std::chrono::milliseconds m_forceCameraTimeThreshold;
    const std::chrono::milliseconds m_maxExpectedMetadataDelay;
    nx::utils::ElapsedTimer m_waitSenderReportTimer; //< need to wait Sender Report before sending BadCameraTime event
    const std::string m_resourceId;
    const nx::rtp::Sdp::MediaType m_mediaType;
    TimePolicy m_timePolicy = TimePolicy::bindCameraTimeToLocalTime;
    bool m_badCameraTimeState = false;
    nx::utils::TimestampAdjustmentHistory m_adjustmentHistory;
    mutable nx::Mutex m_historicalAdjustmentDeltaMutex;
    std::optional<std::chrono::microseconds> m_historicalAdjustmentDelta;
};

} // namespace nx::streaming::rtp
