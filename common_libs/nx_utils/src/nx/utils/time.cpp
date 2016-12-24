#include "time.h"

using namespace std::chrono;

namespace nx {
namespace utils {

namespace {

static std::chrono::milliseconds utcTimeShift(0);
static std::chrono::milliseconds monotonicTimeShift(0);

} // namespace

std::chrono::system_clock::time_point utcTime()
{
    return system_clock::now() + utcTimeShift;
}

std::chrono::seconds timeSinceEpoch()
{
    return duration_cast<seconds>(utcTime().time_since_epoch());
}

std::chrono::steady_clock::time_point monotonicTime()
{
    return steady_clock::now() + monotonicTimeShift;
}

namespace test {

void ScopedTimeShift::shiftCurrentTime(ClockType clockType, milliseconds diff)
{
    if (clockType == ClockType::system)
        utcTimeShift = diff;
    else if (clockType == ClockType::steady)
        monotonicTimeShift = diff;
}

} // namespace test
} // namespace utils
} // namespace nx
