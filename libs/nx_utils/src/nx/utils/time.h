// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <ctime>
#include <optional>
#include <type_traits>

#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "string.h"

namespace nx {
namespace utils {

/**
 * @return By default, std::chrono::system_clock::now() is returned.
 */
NX_UTILS_API std::chrono::system_clock::time_point utcTime();

/** Should be used instead of ::time(). */
NX_UTILS_API std::chrono::seconds timeSinceEpoch();

/**
 * @return UTC time (time since epoch) in milliseconds.
 */
NX_UTILS_API std::chrono::milliseconds millisSinceEpoch();

/**
 * @return By default, std::chrono::steady_clock::now () is returned.
 */
NX_UTILS_API std::chrono::steady_clock::time_point monotonicTime();

/**
 * @return Returns the number of milliseconds that have elapsed since the system was started.
 * The timer continues to run even if the system is suspended. This is not a high resolution timer:
 * on Windows, it has a resolution of 10-16 ms.
 */
NX_UTILS_API std::chrono::milliseconds systemUptime();

/**
 * @return system time and apply test timeShift value on it. It is used in UT only.
 */
NX_UTILS_API std::chrono::system_clock::time_point systemClockTime();

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
 * On Linux, set system date and time. On other platforms, do nothing, return true.
 * @return false on error or unsupported platform, true on success.
 */
NX_UTILS_API bool setDateTime(qint64 millisecondsSinceEpoch);

/**
 * @return QDateTime object corresponding to the given offset from Unix epoch.
 */
NX_UTILS_API QDateTime fromOffsetSinceEpoch(const std::chrono::nanoseconds& offset);

/**
 * Parses the string of the following format:
 * 1 * DIGIT [ SUFFIX ]
 * SUFFIX = ( "h" | "m" | "s" | "ms" | "us" | "ns" )
 * If suffix was not specified then seconds are assumed.
 *
 * @return parsed time duration, std::nullopt in case of malformed string.
 * NOTE: microseconds and nanoseconds are truncated to milliseconds.
 */
NX_UTILS_API std::optional<std::chrono::milliseconds> parseDuration(const std::string_view& str);

/**
 * Invokes parseDuration(str) and returns its result or defaultValue.
 */
NX_UTILS_API std::chrono::milliseconds parseDurationOr(
    const std::string_view& str,
    std::chrono::milliseconds defaultValue);

namespace detail {

template<typename D>
struct DurationTraits {};

template<typename D>
struct DurationTraitsHelper
{
    using HigherOrderDurationType = D;
};

template<> struct DurationTraits<std::chrono::nanoseconds>:
    public DurationTraitsHelper<std::chrono::microseconds> { static constexpr char kSuffix[] = "ns"; };

template<> struct DurationTraits<std::chrono::microseconds>:
    public DurationTraitsHelper<std::chrono::milliseconds> { static constexpr char kSuffix[] = "us"; };

template<> struct DurationTraits<std::chrono::milliseconds>:
    public DurationTraitsHelper<std::chrono::seconds> { static constexpr char kSuffix[] = "ms"; };

template<> struct DurationTraits<std::chrono::seconds>:
    public DurationTraitsHelper<std::chrono::minutes> { static constexpr char kSuffix[] = "s"; };

template<> struct DurationTraits<std::chrono::minutes>:
    public DurationTraitsHelper<std::chrono::hours> { static constexpr char kSuffix[] = "m"; };

template<> struct DurationTraits<std::chrono::hours>:
    public DurationTraitsHelper<void> { static constexpr char kSuffix[] = "h"; };

} // namespace detail

/**
 * Converts duration to string suffixing it with a corresponding
 * suffix: ns (nanoseconds), us (microseconds), ms (milliseconds), s, m, h.
 * E.g., `std::chrono::milliseconds(60'000)` is represented with a string `1m`.
 * The resulting string can be passed to parseDuration.
 */
template<typename Rep, typename Period>
std::string durationToString(const std::chrono::duration<Rep, Period>& d)
{
    using Traits = detail::DurationTraits<std::decay_t<decltype(d)>>;

    if constexpr (std::is_void_v<typename Traits::HigherOrderDurationType>)
    {
        return nx::utils::buildString(d.count(), Traits::kSuffix);
    }
    else
    {
        const auto higherOrderDuration =
            std::chrono::duration_cast<typename Traits::HigherOrderDurationType>(d);
        if (higherOrderDuration == d)
            return durationToString(higherOrderDuration);

        return nx::utils::buildString(d.count(), Traits::kSuffix);
    }
}

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

//-------------------------------------------------------------------------------------------------

namespace test {

enum class ClockType
{
    system,
    steady
};

/**
 * Provides a way to shift the result of utcTime() or monotonicTime() relative to the system values.
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

    ScopedTimeShift(ScopedTimeShift&& rhs) noexcept:
        m_clockType(rhs.m_clockType),
        m_currentAbsoluteShift(rhs.m_currentAbsoluteShift)
    {
        rhs.m_currentAbsoluteShift = std::chrono::milliseconds::zero();
    }

    ScopedTimeShift& operator=(ScopedTimeShift&& rhs) noexcept
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

/**
 * Provides a way to fix the result of utcTime() and make it independent of the
 * system time.
 * Test-only!
 */
struct NX_UTILS_API ScopedUtcTimeFix
{
    explicit ScopedUtcTimeFix(std::chrono::system_clock::time_point time);
    ~ScopedUtcTimeFix();
};

/**
 * Provides a way to shift the result of monotonicTime() and make it independent of the system time.
 * Test-only!
 */
class NX_UTILS_API ScopedSyntheticMonotonicTime
{
public:
    ScopedSyntheticMonotonicTime(const ScopedSyntheticMonotonicTime&) = delete;
    ScopedSyntheticMonotonicTime& operator=(const ScopedSyntheticMonotonicTime&) = delete;

public:
    ScopedSyntheticMonotonicTime(
        const std::chrono::steady_clock::time_point& initTime = monotonicTime());
    ~ScopedSyntheticMonotonicTime();
    void applyRelativeShift(std::chrono::milliseconds value);
    void applyAbsoluteShift(std::chrono::milliseconds value);

private:
    std::chrono::steady_clock::time_point m_initTime;
};

} // namespace test
} // namespace utils
} // namespace nx

#ifdef _WIN32
inline errno_t gmtime_r(const time_t* time, struct tm* result)
{
    return gmtime_s(result, time);
}
#endif
