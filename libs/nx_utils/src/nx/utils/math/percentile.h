// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <type_traits>

#include <nx/reflect/type_utils.h>

#include "../log/assert.h"

namespace nx::utils::math {

/**
 * Calculates a specified percentile of a number series in O(log(n)) time. Uses min and max heaps internally.
 * Access the element at the percentile in O(1) time.
 */
template<typename ValueType>
requires std::disjunction_v<
    std::is_arithmetic<ValueType>,
    nx::reflect::IsStdChronoDuration<ValueType>>
class Percentile
{
public:
    /**
     * @param percentile a value in the range (0, 1) exclusive.
     */
    Percentile(double percentile): m_percentile(percentile)
    {
        NX_ASSERT(0 < m_percentile && m_percentile < 1);
        if(m_percentile <= 0)
            const_cast<double&>(m_percentile) = 0.1;
        else if (m_percentile >= 1)
            const_cast<double&>(m_percentile) = 0.9;
    }

    std::size_t size() const { return m_belowPercentile.size() + m_abovePercentile.size(); }

    bool empty() const { return m_belowPercentile.empty() && m_abovePercentile.empty(); }

    void clear()
    {
        m_belowPercentile.clear();
        m_abovePercentile.clear();
    }

    // Get the smallest element greater than the given percentile in O(1) time.
    // WARNING: assumes non empty set, elements must be added first via add().
    const ValueType& get() const
    {
        return m_abovePercentile.empty() ? m_belowPercentile.front() : m_abovePercentile.front();
    }

    void add(const ValueType& value)
    {
        if (empty())
        {
            m_belowPercentile.emplace_back(value);
            return;
        }

        if (!m_belowPercentile.empty() && value < m_belowPercentile.front())
        {
            m_belowPercentile.emplace_back(value);
            std::push_heap(m_belowPercentile.begin(), m_belowPercentile.end());
        }
        else
        {
            m_abovePercentile.emplace_back(value);
            std::push_heap(m_abovePercentile.begin(), m_abovePercentile.end(), std::greater<ValueType>());
        }

        const auto expectedSizeBelowPercentile = (std::size_t) (size() * m_percentile);
        if (m_belowPercentile.size() > expectedSizeBelowPercentile)
        {
            m_abovePercentile.emplace_back(m_belowPercentile.front());
            std::push_heap(m_abovePercentile.begin(), m_abovePercentile.end(), std::greater<ValueType>());

            std::pop_heap(m_belowPercentile.begin(), m_belowPercentile.end());
            m_belowPercentile.pop_back();
        }
        else if (m_belowPercentile.size() < expectedSizeBelowPercentile)
        {
            m_belowPercentile.emplace_back(m_abovePercentile.front());
            std::push_heap(m_belowPercentile.begin(), m_belowPercentile.end());

            std::pop_heap(m_abovePercentile.begin(), m_abovePercentile.end(), std::greater<ValueType>());
            m_abovePercentile.pop_back();
        }
    }

private:
    const double m_percentile;
    std::vector<ValueType> m_belowPercentile;
    std::vector<ValueType> m_abovePercentile;
};

} // namespace nx::utils::math
