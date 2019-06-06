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
}

TEST(TreeValueHistoryTest, main)
{
    TreeValueHistory<int> history;
    TreeValueHistoryInserter<int> baseInserter(&history);

    auto singleInserter = baseInserter.subValue("single");
    singleInserter.insert(10);

    auto groupInserter = baseInserter.subValue("group");

    auto aaaGroupInserter = groupInserter.subValue("aaa");
    aaaGroupInserter.insert(20);

    auto bbbGroupInserter = groupInserter.subValue("bbb");
    bbbGroupInserter.insert(30);

    EXPECT_EQ(10, history.value(TreeKey{"single"})->current());
    EXPECT_EQ(20, history.value(TreeKey{"group"}["aaa"])->current());
    EXPECT_EQ(30, history.value(TreeKey{"group"}["bbb"])->current());
}

} // namespace nx::utils::test
