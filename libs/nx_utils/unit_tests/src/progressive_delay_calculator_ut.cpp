#include <gtest/gtest.h>

#include <nx/utils/progressive_delay_calculator.h>

namespace nx::utils::test {

constexpr std::chrono::seconds kInitialDelay(1);
constexpr std::chrono::seconds kMaxDelay(10);

using ProgressiveDelayPolicy = nx::utils::ProgressiveDelayPolicy;
using ProgressiveDelayCalculator = nx::utils::ProgressiveDelayCalculator;

TEST(ProgressiveDelayCalculator, calculateNewDelay)
{
    ProgressiveDelayCalculator calculator({kInitialDelay, /*delayMultiplier*/ 2, kMaxDelay});
    ASSERT_EQ(calculator.calculateNewDelay(), kInitialDelay);
    ASSERT_EQ(calculator.calculateNewDelay(), kInitialDelay * 2);
    ASSERT_EQ(calculator.calculateNewDelay(), kInitialDelay * 4);
    ASSERT_EQ(calculator.calculateNewDelay(), kInitialDelay * 8);
    ASSERT_EQ(calculator.calculateNewDelay(), kMaxDelay);
    ASSERT_EQ(calculator.calculateNewDelay(), kMaxDelay);

    ProgressiveDelayCalculator calculator1 =
        ProgressiveDelayCalculator({kInitialDelay, /*delayMultiplier*/ 0, kMaxDelay});
    ASSERT_EQ(calculator1.calculateNewDelay(), kInitialDelay);
    ASSERT_EQ(calculator1.calculateNewDelay(), kInitialDelay);

    ProgressiveDelayCalculator calculator2 =
        ProgressiveDelayCalculator({kInitialDelay, /*delayMultiplier*/ 1, kMaxDelay});
    ASSERT_EQ(calculator2.calculateNewDelay(), kInitialDelay);
    ASSERT_EQ(calculator2.calculateNewDelay(), kInitialDelay);

    ProgressiveDelayCalculator calculator3 =
        ProgressiveDelayCalculator({kInitialDelay, /*delayMultiplier*/ 10, kInitialDelay});
    ASSERT_EQ(calculator3.calculateNewDelay(), kInitialDelay);
    ASSERT_EQ(calculator3.calculateNewDelay(), kInitialDelay);
}

TEST(ProgressiveDelayCalculator, reset)
{
    ProgressiveDelayCalculator calculator({kInitialDelay, /*delayMultiplier*/ 2, kMaxDelay});

    ASSERT_EQ(calculator.triesMade(), 0);
    ASSERT_EQ(calculator.calculateNewDelay(), kInitialDelay);
    ASSERT_EQ(calculator.calculateNewDelay(), kInitialDelay * 2);

    calculator.reset();
    ASSERT_EQ(calculator.triesMade(), 0);
    ASSERT_EQ(calculator.calculateNewDelay(), kInitialDelay);
    ASSERT_EQ(calculator.calculateNewDelay(), kInitialDelay * 2);
}

} // nx::utils::test
