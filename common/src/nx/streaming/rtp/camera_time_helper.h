#pragma once

#include <QtCore/QMutex>
#include <QtCore/QElapsedTimer>

#include <nx/streaming/rtsp_client.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/time_helper.h>

namespace nx::streaming::rtp {

enum class TimePolicy
{
    bindCameraTimeToLocalTime, //< Use camera NPT time, bind it to local time.
    forceCameraTime, //< Use camera NPT time only.
};

class CameraTimeHelper: private utils::TimeHelper
{
public:
    CameraTimeHelper(const QString& resourceId);
    int64_t getUsecTime(
        int64_t rtpTime, const QnRtspStatistic& statistics, int rtpFrequency, bool primaryStream);
    void setResyncThresholdMsec(int64_t value) { m_resyncThresholdMsec = value; }
    void setTimePolicy(TimePolicy policy) { m_timePolicy = policy; }

private:
    int64_t cameraTimeToLocalTime(
        int64_t cameraUsec, int64_t localTimeUsec, bool usePrivateOffset, bool primaryStream);
    void resetTimeOffset(bool usePrivateOffset);

private:
    int64_t m_resyncThresholdMsec = 1000;
    int64_t m_rtpTimeOffset = 0; //< used in case when no rtcp reports
    TimePolicy m_timePolicy = TimePolicy::bindCameraTimeToLocalTime;
};

} // namespace nx::streaming::rtp
