#include "time_helper.h"

#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/utils/log/log.h>

namespace nx::streaming::rtp {

static const double IGNORE_CAMERA_TIME_THRESHOLD_S = 7.0;
static const double LOCAL_TIME_RESYNC_THRESHOLD_MS = 500;
static const int TIME_RESYNC_THRESHOLD_S = 10;
static const double kMaxRtcpJitterSeconds = 0.15; //< 150 ms

QnMutex TimeHelper::m_camClockMutex;

/** map<resID, <CamSyncInfo, refcount>> */
QMap<QString, QPair<QSharedPointer<TimeHelper::CamSyncInfo>, int>>
    TimeHelper::m_camClock;

TimeHelper::TimeHelper(const QString& resourceId):
    m_localStartTime(0),
    m_resourceId(resourceId)
{
    {
        QnMutexLocker lock(&m_camClockMutex);

        QPair<QSharedPointer<TimeHelper::CamSyncInfo>, int>& val = m_camClock[m_resourceId];
        if (!val.first)
            val.first = QSharedPointer<CamSyncInfo>(new CamSyncInfo());
        m_cameraClockToLocalDiff = val.first;

        // Need refcounter, since QSharedPointer does not provide access to its refcounter.
        ++val.second;
    }

    m_localStartTime = qnSyncTime->currentMSecsSinceEpoch();
    m_timer.restart();
    m_lastWarnTime = 0;
}

TimeHelper::~TimeHelper()
{
    QnMutexLocker lock(&m_camClockMutex);

    QMap<QString, QPair<QSharedPointer<TimeHelper::CamSyncInfo>, int>>::iterator it =
        m_camClock.find(m_resourceId);
    if (it != m_camClock.end())
    {
        if (--it.value().second == 0)
            m_camClock.erase(it);
    }
}

void TimeHelper::setTimePolicy(TimePolicy policy)
{
    m_timePolicy = policy;
}

double TimeHelper::cameraTimeToLocalTime(
    double cameraSecondsSinceEpoch, double currentSecondsSinceEpoch)
{
    QnMutexLocker lock(&m_cameraClockToLocalDiff->mutex);
    if (m_cameraClockToLocalDiff->timeDiff == INT_MAX)
        m_cameraClockToLocalDiff->timeDiff = currentSecondsSinceEpoch - cameraSecondsSinceEpoch;
    return cameraSecondsSinceEpoch + m_cameraClockToLocalDiff->timeDiff;
}

void TimeHelper::reset()
{
    QnMutexLocker lock(&m_cameraClockToLocalDiff->mutex);
    m_cameraClockToLocalDiff->timeDiff = INT_MAX;
}

bool TimeHelper::isLocalTimeChanged()
{
    qint64 elapsed;
    int tryCount = 0;
    qint64 ct;
    do
    {
        elapsed = m_timer.elapsed();
        ct = qnSyncTime->currentMSecsSinceEpoch();
    } while (m_timer.elapsed() != elapsed && ++tryCount < 3);

    qint64 expectedLocalTime = elapsed + m_localStartTime;
    bool timeChanged = qAbs(expectedLocalTime - ct) > LOCAL_TIME_RESYNC_THRESHOLD_MS;
    if (timeChanged || elapsed > 3600)
    {
        m_localStartTime = qnSyncTime->currentMSecsSinceEpoch();
        m_timer.restart();
    }
    return timeChanged;
}

bool TimeHelper::isCameraTimeChanged(
    const QnRtspStatistic& statistics,
    double* outCameraTimeDriftSeconds)
{
    if (statistics.isEmpty())
        return false; //< No camera time provided yet.

    double diff = statistics.localTime - double(statistics.senderReport.ntpTimestamp) / 1000000;
    if (!m_rtcpReportTimeDiff.is_initialized())
        m_rtcpReportTimeDiff = diff;

    *outCameraTimeDriftSeconds = qAbs(diff - *m_rtcpReportTimeDiff);
    bool result = false;
    if (*outCameraTimeDriftSeconds > TIME_RESYNC_THRESHOLD_S)
    {
        result = true; //< Quite big delta. Report time change immediately.
    }
    else if (*outCameraTimeDriftSeconds > kMaxRtcpJitterSeconds)
    {
        if (!m_rtcpJitterTimer.isValid())
            m_rtcpJitterTimer.restart();  //< Low delta. Start monitoring for a camera time drift.
        else if (m_rtcpJitterTimer.hasExpired(std::chrono::seconds(TIME_RESYNC_THRESHOLD_S)))
            result = true;
    }
    else
    {
        m_rtcpJitterTimer.invalidate(); //< Jitter back to normal.
    }

    if (result)
        m_rtcpReportTimeDiff.reset();
    return result;
}

#if defined(DEBUG_TIMINGS)

void TimeHelper::printTime(double jitter)
{
    if (m_statsTimer.elapsed() < 1000)
    {
        m_minJitter = qMin(m_minJitter, jitter);
        m_maxJitter = qMax(m_maxJitter, jitter);
        m_jitterSum += jitter;
        m_jitPackets++;
    }
    else
    {
        if (m_jitPackets > 0)
        {
            NX_INFO(this, lm("camera %1. minJit=%2 ms. maxJit=%3 ms. avgJit=%4 ms")
                .arg(m_resourceId)
                .arg((int) (/*rounding*/ 0.5 + m_minJitter * 1000))
                .arg((int) (/*rounding*/ 0.5 + m_maxJitter * 1000))
                .arg((int) (/*rounding*/ 0.5 + m_jitterSum * 1000 / m_jitPackets)));
        }
        m_statsTimer.restart();
        m_minJitter = INT_MAX;
        m_maxJitter = 0;
        m_jitterSum = 0;
        m_jitPackets = 0;
    }
}

#endif // defined(DEBUG_TIMINGS)

/** Intended for logging. */
static QString deltaMs(double scale, double base, double value)
{
    const int64_t d = (int64_t) (0.5 + scale * (value - base));
    if (d >= 0)
        return lit("+%1 ms").arg(d);
    else
        return lit("%1 ms").arg(d);
}

qint64 TimeHelper::getUsecTime(
    quint32 rtpTime, const QnRtspStatistic& statistics, int frequency, bool recursionAllowed)
{
    #define VERBOSE(S) NX_VERBOSE(this, lm("%1() %2").args(__func__, (S)))

    const qint64 currentUs = qnSyncTime->currentUSecsSinceEpoch();
    const qint64 currentMs = (/*rounding*/ 500 + currentUs) / 1000;
    if (statistics.isEmpty() || m_timePolicy == TimePolicy::forceLocalTime)
    {
        VERBOSE(lm("-> %2 (%3), resourceId: %1")
            .arg(m_resourceId)
            .arg(currentUs)
            .arg(m_timePolicy == TimePolicy::forceLocalTime
                ? "ignoreCameraTime=true"
                : "empty statistics"));
        return currentUs;
    }

    if (m_timePolicy == TimePolicy::forceCameraTime
        && statistics.ntpOnvifExtensionTime.is_initialized())
    {
        VERBOSE(lm("-> %2 (%3), resourceId: %1")
            .arg(m_resourceId)
            .arg(currentUs)
            .arg("got time from Onvif NTP extension"));

        return statistics.ntpOnvifExtensionTime->count();
    }

    const double currentSeconds = currentMs / 1000.0;
    const int rtpTimeDiff = rtpTime - statistics.senderReport.rtpTimestamp;
    const double cameraSeconds =
        double(statistics.senderReport.ntpTimestamp) / 1000000 + rtpTimeDiff / (double) frequency;

    const bool gotNewStatistics =
        statistics.senderReport.ntpTimestamp != m_statistics.senderReport.ntpTimestamp ||
        statistics.senderReport.rtpTimestamp != m_statistics.senderReport.rtpTimestamp ||
        m_statistics.isEmpty();

    if (gotNewStatistics)
    {
        m_statistics = statistics;
        QnMutexLocker lock(&m_cameraClockToLocalDiff->mutex);
        m_cameraClockToLocalDiff->timeDiff = currentSeconds - cameraSeconds;
    }

    if (m_timePolicy == TimePolicy::forceCameraTime)
    {
        VERBOSE(lm("-> %2 (%3), resourceId: %1")
            .arg(m_resourceId)
            .arg(currentUs)
            .arg("ignoreLocalTime=true"));

        return cameraSeconds * 1000000LL;
    }

    const double resultSeconds = cameraTimeToLocalTime(cameraSeconds, currentSeconds);
    const double jitterSeconds = qAbs(resultSeconds - currentSeconds);
    const bool gotInvalidTime = jitterSeconds > TIME_RESYNC_THRESHOLD_S;
    double cameraTimeDriftSeconds = 0;
    const bool cameraTimeChanged = isCameraTimeChanged(statistics, &cameraTimeDriftSeconds);
    const bool localTimeChanged = isLocalTimeChanged();

    VERBOSE(lm("BEGIN: "
        "timestamp %1, rtpTime %2 (%3), nowMs %4, camera_from_nowMs %5, result_from_nowMs %6")
        .args(
            statistics.senderReport.rtpTimestamp,
            rtpTime,
            deltaMs(1000 / (double) frequency, m_prevRtpTime, rtpTime),
            deltaMs(1000, m_prevCurrentSeconds, currentSeconds),
            deltaMs(1000, currentSeconds, cameraSeconds),
            deltaMs(1000, currentSeconds, resultSeconds)));
    m_prevRtpTime = rtpTime;
    m_prevCurrentSeconds = currentSeconds;

    #if defined(DEBUG_TIMINGS)
        printTime(jitter);
    #endif

    if (jitterSeconds > IGNORE_CAMERA_TIME_THRESHOLD_S
        && m_timePolicy == TimePolicy::ignoreCameraTimeIfBigJitter)
    {
        m_timePolicy = TimePolicy::forceLocalTime;
        NX_DEBUG(this, lm("Jitter exceeds %1 s; camera time will be ignored")
            .arg(IGNORE_CAMERA_TIME_THRESHOLD_S));
        VERBOSE(lm("-> %1").arg(currentUs));
        return currentUs;
    }

    if ((cameraTimeChanged || localTimeChanged || gotInvalidTime) && recursionAllowed)
    {
        const qint64 currentUsecTime = getUsecTimer();
        if (currentUsecTime - m_lastWarnTime > 2000 * 1000LL)
        {
            if (cameraTimeChanged)
            {
                NX_DEBUG(this, lm(
                    "Camera time has been changed or receiving latency %1 > %2 seconds. "
                    "Resync time for camera %3")
                    .args(cameraTimeDriftSeconds, kMaxRtcpJitterSeconds, m_resourceId));
            }
            else if (localTimeChanged)
            {
                NX_DEBUG(this, lm(
                    "Local time has been changed. Resync time for camera %1")
                    .arg(m_resourceId));
            }
            else
            {
                NX_DEBUG(this, lm(
                    "RTSP time drift reached %1 seconds. Resync time for camera %2")
                    .args(currentSeconds - resultSeconds, m_resourceId));
            }
            m_lastWarnTime = currentUsecTime;
        }

        reset();
        VERBOSE("Reset, calling recursively");
        const qint64 result =
            getUsecTime(rtpTime, statistics, frequency, /*recursionAllowed*/ false); //< recursion
        VERBOSE(lm("END -> %1 (after recursion)").arg(result));
        return result;
    }
    else
    {
        const qint64 result = (gotInvalidTime ? currentSeconds : resultSeconds) * 1000000LL;
        VERBOSE(lm("END -> %1 (gotInvalidTime: %2)")
            .args(result, gotInvalidTime ? "true" : "false"));
        return result;
    }

    #undef VERBOSE
}

} //nx::streaming::rtp
