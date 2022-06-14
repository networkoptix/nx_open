// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/thread/mutex.h>

#include <QtCore/QElapsedTimer>

#include "datetime.h"
#include "timestamp_adjustment_history.h"

namespace nx {
namespace utils {

class NX_UTILS_API TimeHelper
{
public:
    using GetCurrentTimeFunc = std::function<std::chrono::microseconds()>;

    /**
     * @param getCurrentTimeFunc Getter for reference clock
     */
    TimeHelper(const QString& resourceId, GetCurrentTimeFunc getCurrentTimeFunc);

    virtual ~TimeHelper();

    /**
     * @param cameraTimeUs Camera clock timestamp
     * @return Reference clock timestamp corresponding to `cameraTimeUs`
     */
    qint64 getCurrentTimeUs(qint64 cameraTimeUs);

    QString getResourceId() const { return m_resourceId; }

    const TimestampAdjustmentHistory& getAdjustmentHistory() const;

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
        nx::Mutex mutex;
        qint64 timeDiff = DATETIME_INVALID;
        TimestampAdjustmentHistory adjustmentHistory;

        CamSyncInfo();
    };

    struct TimeHelperGlobalContext;

    GetCurrentTimeFunc m_getTime;
    std::shared_ptr<CamSyncInfo> m_cameraClockToLocalDiff;
    QElapsedTimer m_timer; //< Checking local time.
    std::chrono::microseconds m_localStartTimeUs;

    qint64 m_prevCameraTimeUs = 0;
    qint64 m_prevCurrentUs = 0;
};

} // namespace utils
} // namespace nx
