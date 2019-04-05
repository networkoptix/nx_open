#include "camera_time_helper.h"

#include <utils/common/util.h>
#include <nx/utils/log/log.h>
#include <nx/streaming/nx_streaming_ini.h>

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

CameraTimeHelper::CameraTimeHelper(const std::string& resourceId, const TimeOffsetPtr& offset):
    m_primaryOffset(offset),
    m_resyncThreshold(milliseconds(nxStreamingIni().resyncTresholdMs)),
    m_streamsSyncThreshold(milliseconds(nxStreamingIni().streamsSyncThresholdMs)),
    m_forceCameraTimeThreshold(milliseconds(nxStreamingIni().forceCameraTimeThresholdMs)),
    m_resourceId(resourceId)
{}

void CameraTimeHelper::setTimePolicy(TimePolicy policy)
{
    m_timePolicy = policy;
}

microseconds CameraTimeHelper::getCameraTimestamp(
    uint32_t rtpTime,
    const RtcpSenderReport& senderReport,
    const std::optional<microseconds>& onvifTime,
    int frequency)
{
    // onvif
    if (onvifTime)
        return *onvifTime;

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

microseconds CameraTimeHelper::getTime(
    microseconds currentTime,
    uint32_t rtpTime,
    const RtcpSenderReport& senderReport,
    const std::optional<microseconds>& onvifTime,
    int rtpFrequency,
    bool isPrimaryStream,
    const EventCallback& callback)
{
    bool hasAbsoluteTime = senderReport.ntpTimestamp != 0 || onvifTime.has_value();
    const microseconds cameraTime = getCameraTimestamp(
        rtpTime, senderReport, onvifTime, rtpFrequency);
    bool isCameraTimeAccurate = abs(cameraTime - currentTime) < m_forceCameraTimeThreshold;
    if (m_timePolicy == TimePolicy::forceCameraTime)
        return cameraTime;

    if (m_timePolicy == TimePolicy::useCameraTimeIfCorrect)
    {
        if (hasAbsoluteTime && isCameraTimeAccurate)
        {
            if (m_badCameraTimeState)
            {
                NX_DEBUG(this, "Camera time is back to normal, camera time will used");
                callback(EventType::CameraTimeBackToNormal);
            }
            m_badCameraTimeState = false;
            return cameraTime;
        }
        else if (!m_badCameraTimeState)
        {
            NX_DEBUG(this,
                "ResourceId: %1, camera time is not accurate: %2ms, system time: %3ms"
                ", system time will used",
                m_resourceId, cameraTime.count() / 1000, currentTime.count() / 1000);
            callback(EventType::BadCameraTime);
            m_badCameraTimeState = true;
        }
    }

    bool hasPrimaryOffset = m_primaryOffset && m_primaryOffset->initialized;
    bool isStreamDesync = !isPrimaryStream && hasPrimaryOffset &&
        abs(m_primaryOffset->value.load() + cameraTime - currentTime) > m_streamsSyncThreshold;

    if (isStreamDesync)
        callback(EventType::StreamDesync);

    bool useLocalOffset =
        !hasAbsoluteTime || // no absolute time -> can not sync streams
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
    if ((isPrimaryStream || useLocalOffset) && deviation >= m_resyncThreshold)
    {
        NX_DEBUG(this, "ResourceId: %1. Resync camera time, deviation %2ms, cameraTime: %3ms",
            m_resourceId, deviation.count() / 1000, cameraTime.count() / 1000);
        offset.value = currentTime - cameraTime;
        callback(EventType::ResyncToLocalTime);
    }
    return offset.value.load() + cameraTime;
}

} //nx::streaming::rtp
