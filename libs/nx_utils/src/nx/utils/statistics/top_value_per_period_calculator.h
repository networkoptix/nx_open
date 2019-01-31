#pragma once

#include <chrono>
#include <functional>

#include <nx/utils/data_structures/top_queue.h>
#include <nx/utils/time.h>

namespace nx {
namespace utils {
namespace statistics {

/**
 * For a sequence of values returns maximum value per specified period with time complexity O(1).
 * Comp functor defines what the top value is. E.g., For maximum value, specify std::greater here.
 */
template<typename T, typename Comp>
class TopValuePerPeriodCalculator
{
public:
    TopValuePerPeriodCalculator(std::chrono::milliseconds period);

    void add(T val);
    T top(T defaultValue = T()) const;

private:
    struct ValueWithTimestamp
    {
        T value;
        std::chrono::steady_clock::time_point timestamp;

        ValueWithTimestamp() = delete;

        bool operator<(const ValueWithTimestamp& right) const
        {
            return Comp()(value, right.value);
        }
    };

    struct ValueWithTimestampComp
    {
        bool operator()(const ValueWithTimestamp& left, const ValueWithTimestamp& right)
        {
            return Comp()(left.value, right.value);
        }
    };

    std::chrono::milliseconds m_expirationPeriod;
    std::chrono::milliseconds m_aggregationPeriod;
    typename nx::utils::TopQueue<ValueWithTimestamp, ValueWithTimestampComp> m_values;

    void removeExpiredStatistics();
};

template<typename T, typename Comp>
TopValuePerPeriodCalculator<T, Comp>::TopValuePerPeriodCalculator(
    std::chrono::milliseconds period)
    :
    m_expirationPeriod(period),
    m_aggregationPeriod(period / 100)
{
}

template<typename T, typename Comp>
void TopValuePerPeriodCalculator<T, Comp>::add(T val)
{
    removeExpiredStatistics();

    const auto now = nx::utils::monotonicTime();

    if (m_values.empty() ||
        Comp()(val, m_values.top().value) ||
        now - m_values.top().timestamp > m_aggregationPeriod)
    {
        m_values.push(ValueWithTimestamp{
            std::move(val),
            nx::utils::monotonicTime()});
    }
}

template<typename T, typename Comp>
T TopValuePerPeriodCalculator<T, Comp>::top(T defaultValue) const
{
    const_cast<TopValuePerPeriodCalculator*>(this)->removeExpiredStatistics();

    if (m_values.empty())
        return defaultValue;

    return m_values.top().value;
}

template<typename T, typename Comp>
void TopValuePerPeriodCalculator<T, Comp>::removeExpiredStatistics()
{
    const auto now = nx::utils::monotonicTime();

    while (!m_values.empty() &&
        now - m_values.front().timestamp > m_expirationPeriod)
    {
        m_values.pop();
    }
}

//-------------------------------------------------------------------------------------------------

template<typename T> using MaxValuePerPeriodCalculator =
    TopValuePerPeriodCalculator<T, typename std::greater<T>>;

template<typename T> using MinValuePerPeriodCalculator =
    TopValuePerPeriodCalculator<T, typename std::less<T>>;

} // namespace statistics
} // namespace utils
} // namespace nx
