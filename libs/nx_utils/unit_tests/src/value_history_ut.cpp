#include <gtest/gtest.h>

#include <nx/utils/value_history.h>

namespace nx::utils::test {

TEST(ValueHistoryTest, main)
{
    using namespace std::chrono;
    using Border = ValueHistory<int>::Border;
    ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    ValueHistory<int> history(hours(10));
    const auto values =
        [&history](std::optional<milliseconds> maxAge, Border border)
        {
            std::vector<std::pair<int, int>> values;
            const auto addValue =
                [&values](int value, milliseconds age)
                {
                    values.emplace_back(value, round<hours>(age).count());
                };

            maxAge ? history.forEach(*maxAge, addValue, border) : history.forEach(addValue, border);
            return containerString(values);
        };

    EXPECT_EQ(history.current(), 0);
    EXPECT_EQ(values(std::nullopt, Border::keep()), "none");

    history.update(10);
    EXPECT_EQ(history.current(), 10);
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 10: 0 ) }");

    timeShift.applyRelativeShift(hours(2));
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 10: 2 ) }");

    history.update(20);
    EXPECT_EQ(history.current(), 20);
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 10: 2 ), ( 20: 0 ) }");

    timeShift.applyRelativeShift(hours(2));
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 10: 2 ), ( 20: 2 ) }");

    history.update(30);
    EXPECT_EQ(history.current(), 30);
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 10: 2 ), ( 20: 2 ), ( 30: 0 ) }");

    EXPECT_EQ(values(hours(3), Border::keep()), "{ ( 10: 2 ), ( 20: 2 ), ( 30: 0 ) }");
    EXPECT_EQ(values(hours(3), Border::drop()), "{ ( 20: 2 ), ( 30: 0 ) }");
    EXPECT_EQ(values(hours(3), Border::move()), "{ ( 10: 1 ), ( 20: 2 ), ( 30: 0 ) }");
    EXPECT_EQ(values(hours(3), Border::hardcode(7)), "{ ( 7: 1 ), ( 20: 2 ), ( 30: 0 ) }");

    EXPECT_EQ(values(hours(1), Border::keep()), "{ ( 20: 2 ), ( 30: 0 ) }");
    EXPECT_EQ(values(hours(1), Border::drop()), "{ ( 30: 0 ) }");
    EXPECT_EQ(values(hours(1), Border::move()), "{ ( 20: 1 ), ( 30: 0 ) }");
    EXPECT_EQ(values(hours(1), Border::hardcode(0)), "{ ( 0: 1 ), ( 30: 0 ) }");

    timeShift.applyRelativeShift(hours(15));
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 30: 15 ) }");
    EXPECT_EQ(values(std::nullopt, Border::drop()), "none");
    EXPECT_EQ(values(std::nullopt, Border::move()), "{ ( 30: 10 ) }");
    EXPECT_EQ(values(std::nullopt, Border::hardcode(42)), "{ ( 42: 10 ) }");

    history.update(40);
    EXPECT_EQ(history.current(), 40);
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 30: 15 ), ( 40: 0 ) }");
    EXPECT_EQ(values(std::nullopt, Border::drop()), "{ ( 40: 0 ) }");
    EXPECT_EQ(values(std::nullopt, Border::move()), "{ ( 30: 10 ), ( 40: 0 ) }");
    EXPECT_EQ(values(std::nullopt, Border::hardcode(42)), "{ ( 42: 10 ), ( 40: 0 ) }");

    timeShift.applyRelativeShift(hours(15));
    EXPECT_EQ(history.current(), 40);
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 40: 15 ) }");
    EXPECT_EQ(values(std::nullopt, Border::drop()), "none");
    EXPECT_EQ(values(std::nullopt, Border::move()), "{ ( 40: 10 ) }");
    EXPECT_EQ(values(std::nullopt, Border::hardcode(-1)), "{ ( -1: 10 ) }");

    timeShift.applyRelativeShift(hours(15));
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 40: 30 ) }");
    EXPECT_EQ(values(std::nullopt, Border::drop()), "none");
    EXPECT_EQ(values(std::nullopt, Border::move()), "{ ( 40: 10 ) }");
    EXPECT_EQ(values(std::nullopt, Border::hardcode(0)), "{ ( 0: 10 ) }");
}

} // namespace nx::utils::test
