#include <gtest/gtest.h>

#include <nx_ec/transaction_timestamp.h>

namespace ec2 {

TEST(TransactionTimestamp, addNegative)
{
    Timestamp ts;
    ts.sequence = 0;
    ts.ticks = 10;

    ts += -1;

    ASSERT_EQ(0, ts.sequence);
    ASSERT_EQ(9U, ts.ticks);
}

TEST(TransactionTimestamp, addNegativeOverflow)
{
    Timestamp ts;
    ts.sequence = 1;
    ts.ticks = 1;

    ts += -3;

    ASSERT_EQ(0, ts.sequence);
    ASSERT_EQ(std::numeric_limits<std::uint64_t>::max() - 1, ts.ticks);
}

TEST(TransactionTimestamp, subtractPositive)
{
    Timestamp ts;
    ts.sequence = 0;
    ts.ticks = 10;

    ts -= 1;

    ASSERT_EQ(0, ts.sequence);
    ASSERT_EQ(9U, ts.ticks);
}

TEST(TransactionTimestamp, subtractPositiveOverflow)
{
    Timestamp ts;
    ts.sequence = 1;
    ts.ticks = 1;

    ts -= 3;

    ASSERT_EQ(0, ts.sequence);
    ASSERT_EQ(std::numeric_limits<std::uint64_t>::max() - 1, ts.ticks);
}

} // namespace ec2
