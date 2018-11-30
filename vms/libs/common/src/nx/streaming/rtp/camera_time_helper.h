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
    bindCameraTimeToLocalTime, //< Use camera NPT time, bind it to local time.
    forceCameraTime, //< Use camera NPT time only.
};

struct TimeOffset
{
    std::chrono::microseconds get() { return std::chrono::microseconds(*m_timeOffset); }
    void reset(std::chrono::microseconds value) { m_timeOffset = value.count(); }
    bool hasValue() { return m_timeOffset.has_value(); }

private:
    std::optional<std::atomic<int64_t>> m_timeOffset;
};

using TimeOffsetPtr = std::shared_ptr<TimeOffset>;

class CameraTimeHelper
{
public:
    using CurrentTimeGetter = std::function<std::chrono::microseconds()>;
    CameraTimeHelper(
        const std::string& resourceId,
        const TimeOffsetPtr& offset);

    std::chrono::microseconds getTime(
        std::chrono::microseconds currentTime,
        uint32_t rtpTime,
        const RtcpSenderReport& senderReport,
        const std::optional<std::chrono::microseconds>& onvifTime,
        int rtpFrequency,
        bool isPrimaryStream);

    void setResyncThreshold(std::chrono::milliseconds value) { m_resyncThreshold = value; }
    void setTimePolicy(TimePolicy policy) { m_timePolicy = policy; }

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
    CurrentTimeGetter m_timeGetter;
    TimeOffset m_localOffset; //< used in case when no rtcp reports
    std::chrono::milliseconds m_resyncThreshold {1000};
    std::chrono::milliseconds m_streamsSyncThreshold {5000};
    TimePolicy m_timePolicy = TimePolicy::bindCameraTimeToLocalTime;
    std::string m_resourceId;
    RptTimeLinearizer m_linearizer;
};

} // namespace nx::streaming::rtp
