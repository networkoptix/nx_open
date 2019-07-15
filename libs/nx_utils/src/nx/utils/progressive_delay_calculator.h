#pragma once

#include <chrono>
#include <limits>

namespace nx::utils {

class NX_UTILS_API ProgressiveDelayPolicy
{
public:
    static constexpr std::chrono::milliseconds kNoMaxDelay = std::chrono::milliseconds::zero();

    static constexpr std::chrono::milliseconds kDefaultInitialDelay =
        std::chrono::milliseconds(500);
    static constexpr unsigned int kDefaultDelayMultiplier = 2;
    static constexpr std::chrono::milliseconds kDefaultMaxDelay = std::chrono::minutes(1);
    static constexpr double kDefaultRandomRatio = 0;

    std::chrono::milliseconds initialDelay;
    /**
    * Value of 0 means no multiplier (actually, same as 1).
    */
    unsigned int delayMultiplier;
    /**
    * std::chrono::milliseconds::zero is treated as no limit.
    */
    std::chrono::milliseconds maxDelay;
    /**
     * Value from [0, 1], which defines random uniform dispersion for the delay. E.g. if
     * randomRatio=0.3, the resulting delay will be between 0.7 * X and 1.3 * X, where X is a base
     * delay.
     */
    double randomRatio;

    ProgressiveDelayPolicy();
    ProgressiveDelayPolicy(
        std::chrono::milliseconds initialDelay,
        unsigned int delayMultiplier,
        std::chrono::milliseconds maxDelay,
        double randomRatio);

    bool operator==(const ProgressiveDelayPolicy& rhs) const;
};

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API ProgressiveDelayCalculator
{
public:
    ProgressiveDelayCalculator(const ProgressiveDelayPolicy& delayPolicy);

    std::chrono::milliseconds calculateNewDelay();
    std::chrono::milliseconds currentDelay() const;
    unsigned int triesMade() const;
    void reset();

private:
    const ProgressiveDelayPolicy m_delayPolicy;
    std::chrono::milliseconds m_currentDelay{0};
    std::chrono::milliseconds m_effectiveMaxDelay{0};
    unsigned int m_triesMade{0};
    double m_currentRandomBias{0};
};

} // namespace nx::utils
