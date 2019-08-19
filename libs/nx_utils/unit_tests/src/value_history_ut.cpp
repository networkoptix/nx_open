#include <gtest/gtest.h>

#include <nx/utils/value_history.h>

namespace nx::utils::test {

TEST(ValueHistoryTest, main)
{
    ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);
    ValueHistory<int> history(std::chrono::minutes(10));
    EXPECT_EQ(0, history.current());
    EXPECT_EQ(0, history.last().size());

    history.update(10);
    EXPECT_EQ(10, history.current());
    EXPECT_EQ(1, history.last().size());

    timeShift.applyRelativeShift(std::chrono::minutes(2));
    history.update(20);
    EXPECT_EQ(20, history.current());
    EXPECT_EQ(2, history.last().size());

    timeShift.applyRelativeShift(std::chrono::minutes(4));
    history.update(30);
    EXPECT_EQ(30, history.current());
    EXPECT_EQ(3, history.last().size());
    EXPECT_EQ(2, history.last(std::chrono::minutes(1)).size());

    timeShift.applyRelativeShift(std::chrono::minutes(15));
    history.update(40);
    EXPECT_EQ(40, history.current());
    EXPECT_EQ(2, history.last().size());

    timeShift.applyRelativeShift(std::chrono::hours(1));
    EXPECT_EQ(40, history.current());
    EXPECT_EQ(1, history.last(std::chrono::minutes(1)).size());
}

TEST(TreeValueHistoryTest, main)
{
    TreeValueHistory<QString, int> history;
    const auto baseWriter = history.writer();

    auto singleWriter = baseWriter[QString("single")];
    singleWriter->update(10);

    auto groupWriter = baseWriter[QString("group")];

    auto aaaGroupWriter = groupWriter[QString("aaa")];
    aaaGroupWriter->update(20);

    auto bbbGroupWriter = groupWriter[QString("bbb")];
    bbbGroupWriter->update(30);

    const auto singleReader = history.reader("single");
    ASSERT_TRUE((bool) singleReader);
    EXPECT_EQ(10, singleReader->current());

    const auto groupReader = history.reader("group");
    ASSERT_TRUE((bool) groupReader);
    EXPECT_EQ(20, groupReader[QString("aaa")]->current());
    EXPECT_EQ(30, groupReader[QString("bbb")]->current());

    const auto wrongReader = history.reader("wrong");
    ASSERT_FALSE((bool) wrongReader);
    ASSERT_EQ(wrongReader->current(), 0);
}

} // namespace nx::utils::test
