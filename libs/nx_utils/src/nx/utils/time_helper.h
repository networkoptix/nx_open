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
