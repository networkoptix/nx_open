#include "camera_time_helper.h"

#include <utils/common/util.h>
#include <nx/utils/log/log.h>

namespace nx::streaming::rtp {

using namespace std::chrono;

int64_t CameraTimeHelper::RptTimeLinearizer::linearize(uint32_t rtpTime)
{
    if (rtpTime > 0xc0000000 && m_prevTime < 0x40000000)
        m_highPart -= 0x100000000;
    else if (m_prevTime > 0xc0000000 && rtpTime < 0x40000000)
        m_highPart += 0x100000000;
    m_prevTime = rtpTime;
    return m_highPart + rtpTime;
}

CameraTimeHelper::CameraTimeHelper(
    const std::string& resourceId, const TimeOffsetPtr& offset):
    m_primaryOffset(offset),
    m_resourceId(resourceId)
{}

std::chrono::microseconds CameraTimeHelper::getCameraTimestamp(
    uint32_t rtpTime,
    const RtcpSenderReport& senderReport,
    std::optional<std::chrono::microseconds> onvifTime,
    int frequency)
{
    // onvif
    if (onvifTime.has_value())
        return onvifTime.value();

    // rtcp
    if (senderReport.ntpTimestamp > 0)
    {
        const int64_t rtpTimeDiff =
            int32_t(rtpTime - senderReport.rtpTimestamp) * 1000000LL / frequency;
        return microseconds(senderReport.ntpTimestamp + rtpTimeDiff);
    }

    // rtp
    return microseconds(m_linearizer.linearize(rtpTime) * 1000000LL / frequency);
}

std::chrono::microseconds CameraTimeHelper::getTime(
        std::chrono::microseconds currentTime,
        uint32_t rtpTime,
        const RtcpSenderReport& senderReport,
        std::optional<std::chrono::microseconds> onvifTime,
        int rtpFrequency,
        bool isPrimaryStream)
{
    bool hasAbsoluteTime = senderReport.ntpTimestamp != 0 || onvifTime.has_value();
    const microseconds cameraTime = getCameraTimestamp(
        rtpTime, senderReport, onvifTime, rtpFrequency);
    if (m_timePolicy == TimePolicy::forceCameraTime && hasAbsoluteTime)
        return cameraTime;

    bool streamsDesync = !isPrimaryStream && m_primaryOffset && m_primaryOffset->hasValue() &&
        abs(m_primaryOffset->get() + cameraTime - currentTime) > m_streamsSyncThreshold;

    bool useLocalOffset =
        !hasAbsoluteTime || // no absolute time -> can not sync streams
        streamsDesync || // streams desync exceeds the threshold
        !m_primaryOffset || // primary offset not used
        (!m_primaryOffset->hasValue() && !isPrimaryStream); // primary stream not init offset yet

    TimeOffset& offset = useLocalOffset ? m_localOffset : *m_primaryOffset;
    if (!offset.hasValue())
        offset.reset(currentTime - cameraTime);

    const microseconds deviation = abs(offset.get() + cameraTime - currentTime);
    if ((isPrimaryStream || useLocalOffset) && deviation >= m_resyncThreshold)
    {
        NX_DEBUG(this, "ResourceId: %1. Resync camera time, deviation %2ms, cameraTime: %3ms",
            m_resourceId, deviation.count() / 1000, cameraTime.count() / 1000);
        offset.reset(currentTime - cameraTime);
    }
    return offset.get() + cameraTime;
}

} //nx::streaming::rtp
