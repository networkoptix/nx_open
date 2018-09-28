#include "camera_time_helper.h"

#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/utils/log/log.h>

namespace nx::streaming::rtp {

static int64_t getCameraTimestamp(int64_t rtpTime, const QnRtspStatistic& statistics, int frequency)
{
    // onvif
    if (statistics.ntpOnvifExtensionTime.is_initialized())
        return statistics.ntpOnvifExtensionTime->count();
    // rtcp
    if (statistics.senderReport.ntpTimestamp > 0)
    {
        const int64_t rtpTimeDiff =
            (rtpTime - statistics.senderReport.rtpTimestamp) * 1000000LL / frequency;
        return statistics.senderReport.ntpTimestamp + rtpTimeDiff;
    }
    // rtp
    return rtpTime * 1000000LL / frequency;
}

CameraTimeHelper::CameraTimeHelper(const QString& resourceId):
    TimeHelper(resourceId, [](){ return qnSyncTime->currentTimePoint(); })
{
}

int64_t CameraTimeHelper::cameraTimeToLocalTime(
    int64_t cameraUsec, int64_t localTimeUsec, bool usePrivateOffset, bool primaryStream)
{
    if (!usePrivateOffset)
        return TimeHelper::cameraTimeToLocalTime(cameraUsec, localTimeUsec, primaryStream);

    if (m_rtpTimeOffset == 0)
        m_rtpTimeOffset = localTimeUsec - cameraUsec;
    return m_rtpTimeOffset + cameraUsec;
}

void CameraTimeHelper::resetTimeOffset(bool usePrivateOffset)
{
    if (!usePrivateOffset)
        TimeHelper::reset();
    else
        m_rtpTimeOffset = 0;
}

int64_t CameraTimeHelper::getUsecTime(
    int64_t rtpTime, const QnRtspStatistic& statistics, int frequency, bool primaryStream)
{
    const int64_t cameraUsec = getCameraTimestamp(rtpTime, statistics, frequency);
    if (m_timePolicy == TimePolicy::forceCameraTime && !statistics.isEmpty())
        return cameraUsec;

    bool usePrivateOffset = statistics.isEmpty();
    const int64_t localTimeUsec = qnSyncTime->currentUSecsSinceEpoch();
    const int64_t resultUsec =
        cameraTimeToLocalTime(cameraUsec, localTimeUsec, usePrivateOffset, primaryStream);
    const int64_t deviationSeconds = std::abs(resultUsec - localTimeUsec);
    NX_VERBOSE(this,
        "ResourceId: %1. cameraTime: %2ms, resultTime %3ms, rtpTime %4, rtcpTime %5, ntpTime %6, onvif %7",
        m_resourceId,
        cameraUsec / 1000,
        resultUsec / 1000,
        rtpTime,
        statistics.senderReport.rtpTimestamp,
        statistics.senderReport.ntpTimestamp,
        statistics.ntpOnvifExtensionTime.is_initialized() ?
            statistics.ntpOnvifExtensionTime->count() / 1000 : 0);

    bool primaryStreamAbsent =
        localTimeUsec - TimeHelper::lastPrimaryFrameUs() > MAX_FRAME_DURATION_MS * 1000 * 2;
    bool ableToReset = primaryStream || primaryStreamAbsent;
    bool resetTime = (ableToReset || usePrivateOffset) &&
        deviationSeconds > m_resyncThresholdMsec * 1000;
    if (resetTime || TimeHelper::isLocalTimeChanged())
    {
        NX_DEBUG(this,
            "ResourceId: %1. Resync camera time, deviation %2ms, cameraTime: %3ms",
            m_resourceId, deviationSeconds / 1000, cameraUsec / 1000);
        resetTimeOffset(usePrivateOffset);
        return cameraTimeToLocalTime(cameraUsec, localTimeUsec, usePrivateOffset, primaryStream);
    }
    return resultUsec;
}

} //nx::streaming::rtp
