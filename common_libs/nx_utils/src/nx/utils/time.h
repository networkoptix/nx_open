#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>

#include <chrono>

namespace nx {
namespace utils {

/**
 * @return By default, std::chrono::system_clock::now() is returned.
 */
NX_UTILS_API std::chrono::system_clock::time_point utcTime();

/** Should be used instead of ::time(). */
NX_UTILS_API std::chrono::seconds timeSinceEpoch();

/**
 * @return By default, std::chrono::steady_clock::now () is returned.
 */
NX_UTILS_API std::chrono::steady_clock::time_point monotonicTime();

/**
 * On Linux, set system time zone. On other platforms, do nothing, return true.
 * @param timeZoneId IANA id of a time zone.
 * @return False on error, unsupported timeZoneId or unsupported platform, true on success.
 */
NX_UTILS_API bool setTimeZone(const QString& timeZoneId);

/**
 * @return List of ids of time zones which are supported by setTimeZone().
 */
NX_UTILS_API QStringList getSupportedTimeZoneIds();

/**
 * Get the current time zone id. ATTENTION: This id is NOT guaranteed to be in the result of
 * getSupportedTimeZoneIds(). Also it is not guaranteed to be exactly the one actually set by the
 * previous call to setTimeZone() or by other means, though is guaranteed to have the same
 * UTC offset as the latter.
 */
NX_UTILS_API QString getCurrentTimeZoneId();

/**
 * On Linux, set system date and time. On other plaforms, do nothing, return true.
 * @return false on error or unsupported platform, true on success.
 */
NX_UTILS_API bool setDateTime(qint64 millisecondsSinceEpoch);

/**
 * @return QDateTime object corresponding to the given offset from Unix epoch.
 */
NX_UTILS_API QDateTime fromOffsetSinceEpoch(const std::chrono::nanoseconds& offset);

/**
 * Rounds timePoint down to DurationTypeToRoundTo type.
 */
template<typename DurationTypeToRoundTo>
std::chrono::system_clock::time_point floor(
    std::chrono::system_clock::time_point timePoint)
{
    using namespace std::chrono;
    return system_clock::time_point(
        duration_cast<DurationTypeToRoundTo>(timePoint.time_since_epoch()));
}

namespace test {

enum class ClockType
{
    system,
    steady
};

/**
 * Provides a way to shift result of utcTime() by some value.
 * Test-only!
 */
class ScopedTimeShift
{
public:
    ScopedTimeShift(ClockType clockType):
        m_clockType(clockType),
        m_currentAbsoluteShift(std::chrono::milliseconds::zero())
    {
    }

    ScopedTimeShift(ClockType clockType, std::chrono::milliseconds value):
        m_clockType(clockType),
        m_currentAbsoluteShift(value)
    {
        shiftCurrentTime(m_clockType, m_currentAbsoluteShift);
    }

    ScopedTimeShift(ScopedTimeShift&& rhs):
        m_clockType(rhs.m_clockType),
        m_currentAbsoluteShift(rhs.m_currentAbsoluteShift)
    {
        rhs.m_currentAbsoluteShift = std::chrono::milliseconds::zero();
    }

    ScopedTimeShift& operator=(ScopedTimeShift&& rhs)
    {
        if (&rhs == this)
            return *this;

        m_clockType = rhs.m_clockType;
        m_currentAbsoluteShift = rhs.m_currentAbsoluteShift;
        rhs.m_currentAbsoluteShift = std::chrono::milliseconds::zero();
        return *this;
    }

    ScopedTimeShift(const ScopedTimeShift&) = delete;
    ScopedTimeShift& operator=(const ScopedTimeShift&) = delete;

    ~ScopedTimeShift()
    {
        if (m_currentAbsoluteShift != std::chrono::milliseconds::zero())
            shiftCurrentTime(m_clockType, std::chrono::seconds::zero());
    }

    void applyAbsoluteShift(std::chrono::milliseconds value)
    {
        m_currentAbsoluteShift = value;
        shiftCurrentTime(m_clockType, m_currentAbsoluteShift);
    }

    void applyRelativeShift(std::chrono::milliseconds value)
    {
        m_currentAbsoluteShift += value;
        shiftCurrentTime(m_clockType, m_currentAbsoluteShift);
    }

private:
    ClockType m_clockType;
    std::chrono::milliseconds m_currentAbsoluteShift;

    /**
     * Test-only function!
     * After this call utcTime() will add diff to return value.
     */
    NX_UTILS_API static void shiftCurrentTime(
        ClockType clockType,
        std::chrono::milliseconds diff);
};

} // namespace test
} // namespace utils
} // namespace nx
