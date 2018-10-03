#pragma once

#pragma once

#include <QtCore/QMutex>
#include <QtCore/QElapsedTimer>

#include <nx/streaming/rtsp_client.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/mutex.h>

namespace nx::streaming::rtp {

enum class TimePolicy
{
    bindCameraTimeToLocalTime, //< Use camera NPT time, bind it to local time.
    ignoreCameraTimeIfBigJitter, //< Same as previous, switch to ForceLocalTime if big jitter.
    forceLocalTime, //< Use local time only.
    forceCameraTime, //< Use camera NPT time only.
};

class TimeHelper
{
public:

    TimeHelper(const QString& resId);
    ~TimeHelper();

    /*!
        \note Overflow of \a rtpTime is not handled here, so be sure to update \a statistics often enough (twice per \a rtpTime full cycle)
    */
    qint64 getUsecTime(quint32 rtpTime, const QnRtspStatistic& statistics, int rtpFrequency, bool recursionAllowed = true);
    QString getResourceId() const { return m_resourceId; }

    void setTimePolicy(TimePolicy policy);

private:
    double cameraTimeToLocalTime(double cameraSecondsSinceEpoch, double currentSecondsSinceEpoch);
    bool isLocalTimeChanged();
    bool isCameraTimeChanged(const QnRtspStatistic& statistics, double* outCameraTimeDriftSeconds);
    void reset();
private:
    quint32 m_prevRtpTime = 0;
    quint32 m_prevCurrentSeconds = 0;
    QElapsedTimer m_timer;
    qint64 m_localStartTime;
    //qint64 m_cameraTimeDrift;
    boost::optional<double> m_rtcpReportTimeDiff;
    nx::utils::ElapsedTimer m_rtcpJitterTimer;

    struct CamSyncInfo {
        CamSyncInfo(): timeDiff(INT_MAX) {}
        QnMutex mutex;
        double timeDiff;
    };

    QSharedPointer<CamSyncInfo> m_cameraClockToLocalDiff;
    QString m_resourceId;

    static QnMutex m_camClockMutex;
    static QMap<QString, QPair<QSharedPointer<CamSyncInfo>, int> > m_camClock;
    qint64 m_lastWarnTime;
    TimePolicy m_timePolicy = TimePolicy::bindCameraTimeToLocalTime;
    QnRtspStatistic m_statistics;

#ifdef DEBUG_TIMINGS
    void printTime(double jitter);
    QElapsedTimer m_statsTimer;
    double m_minJitter;
    double m_maxJitter;
    double m_jitterSum;
    int m_jitPackets;
#endif

};

} // namespace nx::streaming::rtp
