// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_helper.h"

#include <nx/utils/log/log_main.h>
#include <nx/kit/debug.h>

namespace {

static const std::chrono::milliseconds kLocalTimeResyncThreshold(500);
static const std::chrono::seconds kTimeResyncThreshold(10);
static const std::chrono::seconds kMaxAdjustmentHistoryRecordAge(30);

} // namespace

namespace nx {
namespace utils {

struct TimeHelper::TimeHelperGlobalContext
{
    nx::Mutex camClockMutex;
    QMap<QString, std::shared_ptr<CamSyncInfo>> camClock;

    static TimeHelperGlobalContext& instance()
    {
        static TimeHelperGlobalContext inst;
        return inst;
    }
};

TimeHelper::CamSyncInfo::CamSyncInfo():
    adjustmentHistory(kMaxAdjustmentHistoryRecordAge)
{
}

TimeHelper::TimeHelper(const QString& resourceId, GetCurrentTimeFunc getTime):
    m_resourceId(resourceId),
    m_getTime(getTime)
{
    {
        NX_MUTEX_LOCKER lock(&TimeHelperGlobalContext::instance().camClockMutex);

        auto& syncInfo = TimeHelperGlobalContext::instance().camClock[m_resourceId];
        if (!syncInfo)
            syncInfo = std::make_shared<CamSyncInfo>();
        m_cameraClockToLocalDiff = syncInfo;
    }
}

TimeHelper::~TimeHelper()
{
    NX_MUTEX_LOCKER lock(&TimeHelperGlobalContext::instance().camClockMutex);

    m_cameraClockToLocalDiff.reset();
    auto it = TimeHelperGlobalContext::instance().camClock.find(m_resourceId);
    if (it != TimeHelperGlobalContext::instance().camClock.end())
    {
        if (it.value().use_count() == 1)
            TimeHelperGlobalContext::instance().camClock.erase(it);
    }
}

void TimeHelper::reset()
{
    auto& syncInfo = *m_cameraClockToLocalDiff;

    {
        NX_MUTEX_LOCKER lock(&syncInfo.mutex);
        syncInfo.timeDiff = DATETIME_INVALID;
    }

    syncInfo.adjustmentHistory.reset();
}

qint64 TimeHelper::cameraTimeToLocalTime(qint64 cameraTimeUs, qint64 currentTimeUs) const
{
    NX_MUTEX_LOCKER lock(&m_cameraClockToLocalDiff->mutex);
    if (m_cameraClockToLocalDiff->timeDiff == DATETIME_INVALID)
        m_cameraClockToLocalDiff->timeDiff = currentTimeUs - cameraTimeUs;
    return cameraTimeUs + m_cameraClockToLocalDiff->timeDiff;
}

bool TimeHelper::isLocalTimeChanged()
{
    const std::chrono::microseconds currentTimeUs = m_getTime();
    const std::chrono::microseconds expectedLocalTimeUs =
        m_timer.isValid() ? std::chrono::milliseconds(m_timer.elapsed()) + m_localStartTimeUs : currentTimeUs;
    const std::chrono::microseconds jitterUs(qAbs(expectedLocalTimeUs.count() - currentTimeUs.count()));
    const bool isTimeChanged = jitterUs > kLocalTimeResyncThreshold;
    if (!m_timer.isValid() || isTimeChanged)
    {
        m_localStartTimeUs = currentTimeUs;
        m_timer.restart();
    }
    return isTimeChanged;
}

/** Intended for logging. */
static QString deltaMs(qint64 baseUs, qint64 valueUs)
{
    const int64_t deltaMs = (valueUs - baseUs) / 1000;
    if (deltaMs >= 0)
        return QString("+%1 ms").arg(deltaMs);
    else
        return QString("%1 ms").arg(deltaMs);
}

qint64 TimeHelper::getCurrentTimeUs(const qint64 cameraTimeUs)
{
    quint64 currentTimeUs = getTimeUsInternal(cameraTimeUs, /*recursionAllowed*/ true);

    m_cameraClockToLocalDiff->adjustmentHistory.record(
        std::chrono::microseconds(cameraTimeUs),
        std::chrono::microseconds(currentTimeUs - cameraTimeUs));

    return currentTimeUs;
}

const TimestampAdjustmentHistory& TimeHelper::getAdjustmentHistory() const
{
    return m_cameraClockToLocalDiff->adjustmentHistory;
}

qint64 TimeHelper::getTimeUsInternal(const qint64 cameraTimeUs, bool recursionAllowed)
{
    const qint64 currentUs = m_getTime().count();
    qint64 resultUs = cameraTimeToLocalTime(cameraTimeUs, currentUs);
    const std::chrono::microseconds jitterUs(qAbs(resultUs - currentUs));
    const bool gotInvalidTime = jitterUs > kTimeResyncThreshold;
    const bool localTimeChanged = isLocalTimeChanged();

    NX_VERBOSE(this, nx::format("BEGIN: "
        "Camera time %1 (%2), nowMs %3, camera_from_nowMs %5, result_from_nowMs %6")
        .args(
            cameraTimeUs,
            deltaMs(m_prevCameraTimeUs, cameraTimeUs),
            deltaMs(m_prevCurrentUs, currentUs),
            deltaMs(currentUs, cameraTimeUs),
            deltaMs(currentUs, resultUs)));
    m_prevCameraTimeUs = cameraTimeUs;
    m_prevCurrentUs = currentUs;

    if (!recursionAllowed)
        return gotInvalidTime ? currentUs : resultUs;

    if (localTimeChanged || gotInvalidTime)
    {
        if (localTimeChanged)
        {
            NX_VERBOSE(this, nx::format(
                "Local time has been changed. Resync time for camera %1")
                .arg(m_resourceId));
        }
        else
        {
            NX_VERBOSE(this, nx::format(
                "Device time drift has reached %1 ms. Resync time for device %2")
                .args(jitterUs/1000, m_resourceId));
        }

        reset();
        NX_VERBOSE(this, "Reset, calling recursively");
        resultUs = getTimeUsInternal(cameraTimeUs, /*recursionAllowed*/ false);
        NX_VERBOSE(this, nx::format("END -> %1 (after recursion)").arg(resultUs));
    }
    return resultUs;
}

} // namespace utils
} // namespace nx
