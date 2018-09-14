#include <gtest/gtest.h>

#include <nx/utils/math/cycled_range.h>

namespace nx {
namespace utils {
namespace math {

namespace {

template<typename Number>
struct TestCase
{
    std::pair<Number, Number> range;
    Number initialPosition;
    Number addition;
    Number resultPosition;
};

static const std::vector<TestCase<int>> kIntegerTestCases = {
    {{0, 100}, 0, 50, 50},
    {{-100, 0}, -80, 20, -60},
    {{20, 100}, 50, 30, 80},
    {{-100, -20}, -90, -5, -95},

    {{0, 100}, 0, 120, 20},
    {{-100, 0}, -80, 110, -70},
    {{20, 100}, 50, 125, 95},
    {{-100, -20}, -90, -85, -95},

    {{0, 100}, 0, 220, 20},
    {{-100, 0}, -80, 310, -70},
    {{20, 100}, 50, 205, 95},
    {{-100, -20}, -90, -245, -95},
};

static const std::vector<TestCase<double>> kFloatingTestCases = {
    {{0, 10.5}, 0, 5, 5},
    {{-10.5, 0}, 0, 5, -5.5},
    {{2.7, 10.4}, 5.5, 2.0, 7.5},
    {{-10.7, 0.4}, -5.5, -2.0, -7.5},

    {{0, 10.5}, 0, 15, 4.5},
    {{2.7, 10.4}, 5.5, 12.0, 9.8},
    {{-10.7, 0.4}, -5.5, -12.0, -6.4},
};

template<typename Number>
void runTest(const std::vector<TestCase<Number>>& testCases)
{
    for (const auto& testCase: testCases)
    {
        CycledRange<Number> range(
            testCase.initialPosition,
            testCase.range.first,
            testCase.range.second);

        range += testCase.addition;
        ASSERT_DOUBLE_EQ(*range.position(), testCase.resultPosition);
    }
}

} // namespace

TEST(CycledRange, integerRange)
{
    runTest(kIntegerTestCases);
}

TEST(CycledRange, floatingRange)
{
    runTest(kFloatingTestCases);
}

} // namespace math
} // namespace utils
} // namespace nx
