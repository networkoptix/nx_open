#include <gtest/gtest.h>

#include <nx/utils/time.h>

namespace nx {
namespace utils {

TEST(Time, timeSinceEpoch_equals_time_t)
{
    // TODO: #ak take into account an error.

    const auto t1 = nx::utils::timeSinceEpoch();
    const auto t2 = ::time(NULL);

    ASSERT_EQ(t2, t1.count());
}

} // namespace utils
} // namespace nx
