#include "progressive_delay_calculator.h"

#include <nx/utils/random.h>
#include <nx/utils/log/assert.h>

namespace nx::utils {

constexpr std::chrono::milliseconds ProgressiveDelayPolicy::kNoMaxDelay;
constexpr std::chrono::milliseconds ProgressiveDelayPolicy::kDefaultInitialDelay;
constexpr std::chrono::milliseconds ProgressiveDelayPolicy::kDefaultMaxDelay;

ProgressiveDelayPolicy::ProgressiveDelayPolicy():
    initialDelay(kDefaultInitialDelay),
    delayMultiplier(kDefaultDelayMultiplier),
    maxDelay(kDefaultMaxDelay),
    randomRatio(kDefaultRandomRatio)
{
}

ProgressiveDelayPolicy::ProgressiveDelayPolicy(
    std::chrono::milliseconds initialDelay,
    unsigned int delayMultiplier,
    std::chrono::milliseconds maxDelay,
    double randomRatio)
    :
    initialDelay(initialDelay),
    delayMultiplier(delayMultiplier),
    maxDelay(maxDelay),
    randomRatio(randomRatio)
{
    NX_ASSERT(randomRatio >= 0 && randomRatio <= 1);
}

bool ProgressiveDelayPolicy::operator==(const ProgressiveDelayPolicy& rhs) const
{
    return initialDelay == rhs.initialDelay
        && delayMultiplier == rhs.delayMultiplier
        && maxDelay == rhs.maxDelay
        && randomRatio == rhs.randomRatio;
}

//-------------------------------------------------------------------------------------------------


ProgressiveDelayCalculator::ProgressiveDelayCalculator(
    const ProgressiveDelayPolicy& delayPolicy)
    :
    m_delayPolicy(delayPolicy)
{
    reset();
}

std::chrono::milliseconds ProgressiveDelayCalculator::calculateNewDelay()
{
    if ((m_triesMade > 0) &&
        (m_delayPolicy.delayMultiplier > 0) &&
        (m_currentDelay < m_effectiveMaxDelay))
    {
        auto newDelay = m_currentDelay * m_delayPolicy.delayMultiplier;
        if (newDelay == std::chrono::milliseconds::zero())
            newDelay = std::chrono::milliseconds(1);
        if (newDelay < m_currentDelay)
        {
            //overflow
            newDelay = std::chrono::milliseconds::max();
        }
        m_currentDelay = newDelay;
        if (m_currentDelay > m_effectiveMaxDelay)
            m_currentDelay = m_effectiveMaxDelay;
    }

    ++m_triesMade;

    if (m_delayPolicy.randomRatio != 0)
        m_currentRandomBias = nx::utils::random::numberDelta(1.0, m_delayPolicy.randomRatio);

    return currentDelay();
}

std::chrono::milliseconds ProgressiveDelayCalculator::currentDelay() const
{
    if (m_delayPolicy.randomRatio == 0)
        return m_currentDelay;

    return std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(
        m_currentRandomBias * m_currentDelay.count()));
}

unsigned int ProgressiveDelayCalculator::triesMade() const
{
    return m_triesMade;
}

void ProgressiveDelayCalculator::reset()
{
    m_currentDelay = m_delayPolicy.initialDelay;
    m_effectiveMaxDelay =
        m_delayPolicy.maxDelay == ProgressiveDelayPolicy::kNoMaxDelay
        ? std::chrono::milliseconds::max()
        : m_delayPolicy.maxDelay;
    m_triesMade = 0;
}

} // namespace nx::utils
