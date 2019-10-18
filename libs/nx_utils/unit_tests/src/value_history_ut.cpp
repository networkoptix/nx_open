#include <gtest/gtest.h>

#include <nx/utils/value_history.h>

namespace nx::utils::test {

TEST(ValueHistoryTest, main)
{
    using namespace std::chrono;
    ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    ValueHistory<int> history(hours(10));
    const auto values =
        [&history](std::optional<milliseconds> maxAge)
        {
            std::vector<std::pair<int, int>> values;
            const auto addValue =
                [&values](int value, milliseconds age)
                {
                    values.emplace_back(value, round<hours>(age).count());
                };

            maxAge ? history.forEach(*maxAge, addValue) : history.forEach(addValue);
            return containerString(values);
        };

    EXPECT_EQ(history.current(), 0);
    EXPECT_EQ(values(std::nullopt), "none");

    history.update(10);
    EXPECT_EQ(history.current(), 10);
    EXPECT_EQ(values(std::nullopt), "{ ( 10: 0 ) }");

    timeShift.applyRelativeShift(hours(2));
    EXPECT_EQ(values(std::nullopt), "{ ( 10: 2 ) }");

    history.update(20);
    EXPECT_EQ(history.current(), 20);
    EXPECT_EQ(values(std::nullopt), "{ ( 10: 2 ), ( 20: 0 ) }");

    timeShift.applyRelativeShift(hours(2));
    EXPECT_EQ(values(std::nullopt), "{ ( 10: 2 ), ( 20: 2 ) }");

    history.update(30);
    EXPECT_EQ(history.current(), 30);
    EXPECT_EQ(values(std::nullopt), "{ ( 10: 2 ), ( 20: 2 ), ( 30: 0 ) }");
    EXPECT_EQ(values(hours(3)), "{ ( 10: 1 ), ( 20: 2 ), ( 30: 0 ) }");
    EXPECT_EQ(values(hours(1)), "{ ( 20: 1 ), ( 30: 0 ) }");

    timeShift.applyRelativeShift(hours(15));
    EXPECT_EQ(values(std::nullopt), "{ ( 30: 10 ) }");

    history.update(40);
    EXPECT_EQ(history.current(), 40);
    EXPECT_EQ(values(std::nullopt), "{ ( 30: 10 ), ( 40: 0 ) }");

    timeShift.applyRelativeShift(hours(15));
    EXPECT_EQ(values(std::nullopt), "{ ( 40: 10 ) }");

    EXPECT_EQ(history.current(), 40);
    EXPECT_EQ(values(std::nullopt), "{ ( 40: 10 ) }");

    timeShift.applyRelativeShift(hours(15));
    EXPECT_EQ(values(std::nullopt), "{ ( 40: 10 ) }");
}

} // namespace nx::utils::test
