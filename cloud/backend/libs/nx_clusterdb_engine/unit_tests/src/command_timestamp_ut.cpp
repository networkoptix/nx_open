#include <limits>

#include <gtest/gtest.h>

#include <nx/clusterdb/engine/timestamp.h>

namespace nx::clusterdb::engine::test {

TEST(Timestamp, addNegative)
{
    Timestamp ts;
    ts.sequence = 0;
    ts.ticks = 10;

    ts += std::chrono::milliseconds(-1);

    ASSERT_EQ(0U, ts.sequence);
    ASSERT_EQ(9U, ts.ticks);
}

TEST(Timestamp, addNegativeOverflow)
{
    Timestamp ts;
    ts.sequence = 1;
    ts.ticks = 1;

    ts += std::chrono::milliseconds(-3);

    ASSERT_EQ(0U, ts.sequence);
    ASSERT_EQ(std::numeric_limits<std::uint64_t>::max() - 1, ts.ticks);
}

TEST(Timestamp, subtractPositive)
{
    Timestamp ts;
    ts.sequence = 0;
    ts.ticks = 10;

    ts -= std::chrono::milliseconds(1);

    ASSERT_EQ(0U, ts.sequence);
    ASSERT_EQ(9U, ts.ticks);
}

TEST(Timestamp, subtractPositiveOverflow)
{
    Timestamp ts;
    ts.sequence = 1;
    ts.ticks = 1;

    ts -= std::chrono::milliseconds(3);

    ASSERT_EQ(0U, ts.sequence);
    ASSERT_EQ(std::numeric_limits<std::uint64_t>::max() - 1, ts.ticks);
}

} // namespace nx::clusterdb::engine::test
