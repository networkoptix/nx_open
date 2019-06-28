#include <gtest/gtest.h>

#include <nx/utils/progressive_delay_calculator.h>

constexpr std::chrono::milliseconds initialDelay(1000);
constexpr std::chrono::milliseconds maxDelay(10000);

using ProgressiveDelayPolicy = nx::utils::ProgressiveDelayPolicy;
using ProgressiveDelayCalculator = nx::utils::ProgressiveDelayCalculator;

TEST(ProgressiveDelayCalculator, calculateNewDelay)
{

    ProgressiveDelayCalculator calculator({initialDelay, /*delayMultiplier*/ 2, maxDelay});
    ASSERT_EQ(calculator.calculateNewDelay(), initialDelay);
    ASSERT_EQ(calculator.calculateNewDelay(), initialDelay * 2);
    ASSERT_EQ(calculator.calculateNewDelay(), initialDelay * 4);
    ASSERT_EQ(calculator.calculateNewDelay(), initialDelay * 8);
    ASSERT_EQ(calculator.calculateNewDelay(), maxDelay);
    ASSERT_EQ(calculator.calculateNewDelay(), maxDelay);

    ProgressiveDelayCalculator calculator1 =
        ProgressiveDelayCalculator({initialDelay, /*delayMultiplier*/ 0, maxDelay});
    ASSERT_EQ(calculator1.calculateNewDelay(), initialDelay);
    ASSERT_EQ(calculator1.calculateNewDelay(), initialDelay);

    ProgressiveDelayCalculator calculator2 =
        ProgressiveDelayCalculator({initialDelay, /*delayMultiplier*/ 1, maxDelay});
    ASSERT_EQ(calculator2.calculateNewDelay(), initialDelay);
    ASSERT_EQ(calculator2.calculateNewDelay(), initialDelay);

    ProgressiveDelayCalculator calculator3 =
        ProgressiveDelayCalculator({initialDelay, /*delayMultiplier*/ 10, initialDelay});
    ASSERT_EQ(calculator3.calculateNewDelay(), initialDelay);
    ASSERT_EQ(calculator3.calculateNewDelay(), initialDelay);
}

TEST(ProgressiveDelayCalculator, reset)
{
    ProgressiveDelayCalculator calculator({initialDelay, /*delayMultiplier*/ 2, maxDelay});

    ASSERT_EQ(calculator.triesMade(), 0);
    ASSERT_EQ(calculator.calculateNewDelay(), initialDelay);
    ASSERT_EQ(calculator.calculateNewDelay(), initialDelay * 2);

    calculator.reset();
    ASSERT_EQ(calculator.triesMade(), 0);
    ASSERT_EQ(calculator.calculateNewDelay(), initialDelay);
    ASSERT_EQ(calculator.calculateNewDelay(), initialDelay * 2);
}
