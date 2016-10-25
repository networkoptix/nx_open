#include "time.h"

using namespace std::chrono;

namespace nx {
namespace utils {

namespace {
static std::chrono::milliseconds timeShift(0);
} // namespace

std::chrono::system_clock::time_point utcTime()
{
    return system_clock::now() + timeShift;
}

std::chrono::seconds timeSinceEpoch()
{
    return duration_cast<seconds>(utcTime().time_since_epoch());
}

namespace test {

void ScopedTimeShift::shiftCurrentTime(milliseconds diff)
{
    timeShift = diff;
}

} // namespace test

} // namespace utils
} // namespace nx
