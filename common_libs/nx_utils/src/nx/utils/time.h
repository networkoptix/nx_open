#pragma once

#include <chrono>

namespace nx {
namespace utils {

NX_UTILS_API std::chrono::system_clock::time_point utcTime();
/** Should be used instead of \a ::time(). */
NX_UTILS_API std::chrono::seconds timeSinceEpoch();

namespace test {

/** 
 * Test-only class!
 * Provides a way to shift result of \a utcTime() by some value.
 */
class ScopedTimeShift
{
public:
    ScopedTimeShift():
        m_currentAbsoluteShift(0)
    {
    }

    ScopedTimeShift(std::chrono::milliseconds value):
        m_currentAbsoluteShift(value)
    {
        shiftCurrentTime(m_currentAbsoluteShift);
    }

    ~ScopedTimeShift()
    {
        if (m_currentAbsoluteShift != std::chrono::milliseconds::zero())
            shiftCurrentTime(std::chrono::seconds::zero());
    }

    void applyAbsoluteShift(std::chrono::milliseconds value)
    {
        m_currentAbsoluteShift = value;
        shiftCurrentTime(m_currentAbsoluteShift);
    }

    void applyRelativeShift(std::chrono::milliseconds value)
    {
        m_currentAbsoluteShift += value;
        shiftCurrentTime(m_currentAbsoluteShift);
    }

private:
    std::chrono::milliseconds m_currentAbsoluteShift;

    /** 
     * Test-only function!
     * After this call \a utcTime() will add \a diff to return value.
     */
    NX_UTILS_API static void shiftCurrentTime(std::chrono::milliseconds diff);
};

} // namespace test

} // namespace utils
} // namespace nx
