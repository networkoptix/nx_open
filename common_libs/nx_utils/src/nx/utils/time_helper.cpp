#include "time_helper.h"

#include <nx/utils/log/log_main.h>
#include <nx/kit/debug.h>

namespace {

static const std::chrono::milliseconds kLocalTimeResyncThreshold(500);
static const std::chrono::seconds kTimeResyncThreshold(10);

} // namespace

namespace nx {
namespace utils {

QnMutex TimeHelper::m_camClockMutex;
QMap<QString, std::shared_ptr<TimeHelper::CamSyncInfo>> TimeHelper::m_camClock;

TimeHelper::TimeHelper(const QString& resourceId, GetCurrentTimeFunc getTime):
    m_resourceId(resourceId),
    m_getTime(getTime)
{
    {
        QnMutexLocker lock(&m_camClockMutex);

        auto& syncInfo = m_camClock[m_resourceId];
        if (!syncInfo)
            syncInfo = std::make_shared<CamSyncInfo>();
        m_cameraClockToLocalDiff = syncInfo;
    }
}

TimeHelper::~TimeHelper()
{
    QnMutexLocker lock(&m_camClockMutex);
    m_cameraClockToLocalDiff.reset();
    auto it = m_camClock.find(m_resourceId);
    if (it != m_camClock.end())
    {
        if (it.value().use_count() == 1)
            m_camClock.erase(it);
    }
}

void TimeHelper::reset()
{
    QnMutexLocker lock(&m_cameraClockToLocalDiff->mutex);
    m_cameraClockToLocalDiff->timeDiff = DATETIME_INVALID;
}

qint64 TimeHelper::cameraTimeToLocalTime(qint64 cameraTimeUs, qint64 currentTimeUs) const
{
    QnMutexLocker lock(&m_cameraClockToLocalDiff->mutex);
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
    return getTimeUsInternal(cameraTimeUs, /*recursionAllowed*/ true);
}

qint64 TimeHelper::getTimeUsInternal(const qint64 cameraTimeUs, bool recursionAllowed)
{
    const qint64 currentUs = m_getTime().count();
    qint64 resultUs = cameraTimeToLocalTime(cameraTimeUs, currentUs);
    const std::chrono::microseconds jitterUs(qAbs(resultUs - currentUs));
    const bool gotInvalidTime = jitterUs > kTimeResyncThreshold;
    const bool localTimeChanged = isLocalTimeChanged();

    NX_VERBOSE(this, lm("BEGIN: "
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
            NX_VERBOSE(this, lm(
                "Local time has been changed. Resync time for camera %1")
                .arg(m_resourceId));
        }
        else
        {
            NX_VERBOSE(this, lm(
                "Device time drift has reached %1 ms. Resync time for device %2")
                .args(jitterUs/1000, m_resourceId));
        }

        reset();
        NX_VERBOSE(this, "Reset, calling recursively");
        resultUs = getTimeUsInternal(cameraTimeUs, /*recursionAllowed*/ false);
        NX_VERBOSE(this, lm("END -> %1 (after recursion)").arg(resultUs));
    }
    return resultUs;
}

/*static*/ qint64 TimeHelper::unloopCameraPtsWithModulus(
    GetCurrentTimeFunc getCurrentTimeFunc,
    qint64 absentPtsUsValue,
    int modulusUs,
    qint64 ptsUs,
    qint64 prevPtsUs,
    qint64* periodStartUs)
{
    if (ptsUs < 0)
        NX_PRINT << "WARNING: PTS is less than zero: " << ptsUs;

    if (prevPtsUs == absentPtsUsValue)
    {
        // First frame received.
        const qint64 nowUs = std::chrono::microseconds(getCurrentTimeFunc()).count();
        *periodStartUs = (nowUs / modulusUs - 1) * modulusUs;
        NX_PRINT << "First frame: ptsUs " << ptsUs << ", periodStartUs " << *periodStartUs;
    }
    else if (prevPtsUs > ptsUs)
    {
        // Looping - first frame of the period received.
        *periodStartUs += modulusUs;
        NX_PRINT << "Looping: ptsUs " << ptsUs << ", periodStartUs " << *periodStartUs;
    }

    return *periodStartUs + ptsUs % modulusUs;
}

} // namespace utils
} // namespace nx
