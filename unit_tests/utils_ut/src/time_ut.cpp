#include <chrono>
#include <cstdlib>

#include <gtest/gtest.h>

#include <nx/utils/time.h>

namespace nx {
namespace utils {

TEST(Time, timeSinceEpoch_equals_time_t)
{
    using namespace std::chrono;

    const auto testStart = steady_clock::now();

    const auto t1 = nx::utils::timeSinceEpoch();
    const auto t2 = ::time(NULL);

    const auto testRunTime = steady_clock::now() - testStart;

    ASSERT_LE(
        std::abs(t2 - duration_cast<seconds>(t1).count()),
        duration_cast<seconds>(testRunTime).count()+1);
}

} // namespace utils
} // namespace nx
