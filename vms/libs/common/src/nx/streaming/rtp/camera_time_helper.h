#pragma once

#include <optional>
#include <memory>
#include <chrono>
#include <string>
#include <atomic>
#include <functional>

#include <nx/streaming/rtp/rtcp.h>

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

class CameraTimeHelper
{
public:
    enum class EventType
    {
        BadCameraTime,
        CameraTimeBackToNormal,
        StreamDesync,
        ResyncToLocalTime,
    };

    using EventCallback = std::function<void(EventType event)>;

public:
    CameraTimeHelper(const std::string& resourceId, const TimeOffsetPtr& offset);

    std::chrono::microseconds getTime(
        std::chrono::microseconds currentTime,
        uint32_t rtpTime,
        const RtcpSenderReport& senderReport,
        const std::optional<std::chrono::microseconds>& onvifTime,
        int rtpFrequency,
        bool isPrimaryStream,
        const EventCallback& callback = [](EventType /*event*/){});

    void setResyncThreshold(std::chrono::milliseconds value) { m_resyncThreshold = value; }
    void setTimePolicy(TimePolicy policy);

private:
    struct RptTimeLinearizer
    {
        int64_t linearize(uint32_t rtpTime);

    private:
        uint32_t m_prevTime = 0;
        int64_t m_highPart = 0;
    };

    std::chrono::microseconds getCameraTimestamp(
        uint32_t rtpTime,
        const RtcpSenderReport& senderReport,
        const std::optional<std::chrono::microseconds>& onvifTime,
        int frequency);

private:
    TimeOffsetPtr m_primaryOffset;
    TimeOffset m_localOffset; //< used in case when no rtcp reports
    std::chrono::milliseconds m_resyncThreshold{1000};
    std::chrono::milliseconds m_streamsSyncThreshold{5000};
    std::chrono::milliseconds m_forceCameraTimeThreshold{10000};
    TimePolicy m_timePolicy = TimePolicy::bindCameraTimeToLocalTime;
    std::string m_resourceId;
    RptTimeLinearizer m_linearizer;
    bool m_badCameraTimeState = false;
};

} // namespace nx::streaming::rtp
