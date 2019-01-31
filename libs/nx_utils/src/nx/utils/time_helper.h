#pragma once

#include <QtCore/QElapsedTimer>
#include <nx/utils/thread/mutex.h>
#include <memory>
#include "datetime.h"

namespace nx {
namespace utils {

class NX_UTILS_API TimeHelper
{
public:
    using GetCurrentTimeFunc = std::function<std::chrono::microseconds()>;

    TimeHelper(const QString& resourceId, GetCurrentTimeFunc getCurrentTimeFunc);
    virtual ~TimeHelper();

    qint64 getCurrentTimeUs(qint64 cameraTimeUs);
    QString getResourceId() const { return m_resourceId; }

    /**
     * Assume the camera sends PTSes starting from e.g. 0, and looping no later than modulusUs,
     * like testcamera can do with the appropriate options. The goal is to convert PTSes from the
     * camera to "unlooped" PTSes which are monotonous. PTS 0 is converted to a moment with 
     * us-since-epoch divisible by modulusUs, and the first received PTS is converted to a moment
     * in the past which is the closest to the current time. This allows to restore the original
     * PTS by taking the "unlooped" PTS modulo modulusUs.
     * 
     * NOTE: Logging is performed via NX_PRINT, because this function is intended to be used for
     * debugging and experimenting together with other functionality that uses the same log.
     */
    static qint64 unloopCameraPtsWithModulus(
        GetCurrentTimeFunc getCurrentTimeFunc,
        qint64 absentPtsUsValue,
        int modulusUs,
        qint64 ptsUs,
        qint64 prevPtsUs,
        qint64* periodStartUs);

protected:
    void reset();
    qint64 lastPrimaryFrameUs();
    bool isLocalTimeChanged();
    qint64 getTimeUsInternal(qint64 cameraTimeUs, bool recursionAllowed);
    qint64 cameraTimeToLocalTime(qint64 cameraTimeUs, qint64 currentTimeUs) const;
    QString m_resourceId;

private:
    struct CamSyncInfo
    {
        QnMutex mutex;
        qint64 timeDiff = DATETIME_INVALID;
    };

    GetCurrentTimeFunc m_getTime;
    std::shared_ptr<CamSyncInfo> m_cameraClockToLocalDiff;
    static QnMutex m_camClockMutex;
    static QMap<QString, std::shared_ptr<CamSyncInfo>> m_camClock;
    QElapsedTimer m_timer; //< Checking local time.
    std::chrono::microseconds m_localStartTimeUs;

    qint64 m_prevCameraTimeUs = 0;
    qint64 m_prevCurrentUs = 0;
};

} // namespace utils
} // namespace nx
