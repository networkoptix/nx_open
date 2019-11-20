#include <gtest/gtest.h>

#include <QJsonValue>

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/random.h>
#include <nx/utils/value_history.h>

namespace nx::utils::test {

using namespace std::chrono;
using Border = ValueHistory<int>::Border;

class ValueHistoryTest: public testing::Test
{
public:
    ValueHistoryTest():
        timeShift(nx::utils::test::ClockType::steady),
        history(hours(10))
    {
    }

    QString values(std::optional<milliseconds> maxAge, Border border) const
    {
        std::vector<std::pair<int, QString>> values;
        const auto addValue =
            [&values](int value, milliseconds age)
            {
                const auto h = round<hours>(age);
                auto ts = QString::number(h.count());
                if (const auto m = round<minutes>(age) - h; m.count())
                    ts += QString(":%1").arg((int) m.count(), 2, 10, QChar('0'));
                values.emplace_back(value, ts);
            };

        maxAge ? history.forEach(*maxAge, addValue, border) : history.forEach(addValue, border);
        return containerString(values);
    }

protected:
    ScopedTimeShift timeShift;
    ValueHistory<int> history;
};

TEST_F(ValueHistoryTest, normalAge)
{
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

TEST_F(ValueHistoryTest, reducedAge)
{
    nx::kit::IniConfig::Tweaks iniTweaks;
    iniTweaks.set(&nx::utils::ini().valueHistoryAgeDelimiter, double(60)); //< A minute for an hour.

    history.update(10);
    EXPECT_EQ(history.current(), 10);
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 10: 0 ) }");

    timeShift.applyRelativeShift(minutes(5));
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 10: 0:05 ) }");

    history.update(20);
    EXPECT_EQ(history.current(), 20);
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 10: 0:05 ), ( 20: 0 ) }");
    EXPECT_EQ(values(hours(3), Border::move()), "{ ( 10: 0:03 ), ( 20: 0 ) }");

    timeShift.applyRelativeShift(minutes(15));
    EXPECT_EQ(values(std::nullopt, Border::keep()), "{ ( 20: 0:15 ) }");
    EXPECT_EQ(values(hours(5), Border::keep()), "{ ( 20: 0:15 ) }");
    EXPECT_EQ(values(hours(5), Border::move()), "{ ( 20: 0:05 ) }");
    EXPECT_EQ(values(hours(5), Border::drop()), "none");
}

namespace {

struct Times
{
    std::chrono::milliseconds maxAge;
    std::chrono::milliseconds step;
    std::chrono::milliseconds select;
};

class Performance: public testing::TestWithParam<Times> {};

} // namespace

TEST_P(Performance, ForEach)
{
    const auto& times = GetParam();
    ValueHistory<QJsonValue> history(times.maxAge);
    ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);
    for (std::chrono::milliseconds shift(0); shift <= times.maxAge; shift += times.step)
    {
        timeShift.applyRelativeShift(times.step);
        history.update(nx::utils::random::number(0, 10000));
    }

    nx::utils::ElapsedTimer timer(/*started*/ true);
    double sum(0);
    size_t count(0);
    std::chrono::milliseconds duration(0);
    history.forEach(
        times.select,
        [&](const auto& value, const auto& valueDuration)
        {
            count += 1;
            sum += value.toDouble();
            duration += valueDuration;
        },
        ValueHistory<QJsonValue>::Border::keep());

    NX_INFO(this, "Got %1 values in %2", count, timer.elapsed());
    EXPECT_GT(double(count), double(times.select / times.step) * 0.9);
    EXPECT_GT(sum / count, 0);
    EXPECT_LT(sum / count, 10000);
    EXPECT_GT(duration.count(), std::chrono::round<decltype(duration)>(times.select).count() * 0.9);
}

INSTANTIATE_TEST_CASE_P(ValueHistoryTest, Performance, ::testing::Values(
   Times{std::chrono::hours(24), std::chrono::seconds(5), std::chrono::hours(24)},
   Times{std::chrono::hours(24), std::chrono::seconds(5), std::chrono::hours(1)},
   Times{std::chrono::hours(24), std::chrono::milliseconds(10), std::chrono::hours(1)}
));

} // namespace nx::utils::test
