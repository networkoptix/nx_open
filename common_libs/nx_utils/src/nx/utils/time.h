#pragma once

#include <chrono>

namespace nx {
namespace utils {

/**
 * @return By default, std::chrono::system_clock::now () is returned.
 */
NX_UTILS_API std::chrono::system_clock::time_point utcTime();

/** Should be used instead of \a ::time(). */
NX_UTILS_API std::chrono::seconds timeSinceEpoch();

/**
 * @return By default, std::chrono::steady_clock::now () is returned.
 */
NX_UTILS_API std::chrono::steady_clock::time_point monotonicTime();

/**
 * On Linux, get filename of a file which is used to set system time zone.
 * @param timeZoneId IANA id of a time zone.
 * @return On Linux - time zone file name, or a null string if time zone id is not valid; on
 *     other platforms - an empty string.
 */
NX_UTILS_API QString getTimeZoneFile(const QString& timeZoneId);

/**
 * On Linux, set system time zone. On other platforms, do nothing, return true.
 * @param timeZoneId IANA id of a time zone.
 * @return False on error, invalid timeZoneId or unsupported platform, true on success.
 */
NX_UTILS_API bool setTimeZone(const QString& timeZoneId);

/**
 * On Linux, set system date and time. On other plaforms, do nothing, return true.
 * @return false on error or unsupported platform, true on success.
 */
NX_UTILS_API bool setDateTime(qint64 millisecondsSinceEpoch);

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
     * After this call \a utcTime() will add \a diff to return value.
     */
    NX_UTILS_API static void shiftCurrentTime(
        ClockType clockType,
        std::chrono::milliseconds diff);
};

} // namespace test
} // namespace utils
} // namespace nx
