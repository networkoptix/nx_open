#pragma once

#include <algorithm>
#include <type_traits>

#include <nx/utils/log/assert.h>

namespace nx {
namespace utils {

/**
 * Integral type interval, right-open [lower, upper).
 */
template<typename T>
class Interval
{
    static_assert(std::is_integral_v<T>);

public:
    Interval() = default;
    Interval(const Interval& other) = default;
    Interval(T lower, T upper): m_lower(lower), m_upper(std::max(lower, upper)) {}

    explicit Interval(T single): Interval(single, single + 1)
    {
        NX_ASSERT(!isEmpty(), "Arithmetic overflow");
    }

    T lower() const { return m_lower; }
    T upper() const { return m_upper; }

    T length() const { return m_upper - m_lower; }

    bool contains(T value) const { return value >= m_lower && value < m_upper; }

    bool isEmpty() const { return length() <= 0; }
    explicit operator bool() const { return !isEmpty(); }

    bool operator==(const Interval& other) const
    {
        return (m_lower == other.m_lower && m_upper == other.m_upper)
            || (isEmpty() && other.isEmpty());
    }

    bool operator!=(const Interval& other) const
    {
        return !(*this == other);
    }

    bool intersects(const Interval& other) const
    {
        return !intersected(other).isEmpty();
    }

    Interval intersected(const Interval& other) const
    {
        return Interval(std::max(m_lower, other.m_lower), std::min(m_upper, other.m_upper));
    }

    Interval expanded(const Interval& other) const
    {
        if (other.isEmpty())
            return *this;

        if (isEmpty())
            return other;

        return Interval(std::min(m_lower, other.m_lower), std::max(m_upper, other.m_upper));
    }

    Interval expanded(T value) const
    {
        return isEmpty()
            ? Interval(value)
            : Interval(std::min(m_lower, value), std::max(m_upper, value + 1));
    }

    Interval truncatedLeft(T lower) const
    {
        return Interval(std::max(lower, m_lower), m_upper);
    }

    Interval truncatedRight(T upper) const
    {
        return Interval(m_lower, std::min(m_upper, upper));
    }

    Interval shifted(T delta) const
    {
        if (isEmpty())
            return *this;

        const Interval result(m_lower + delta, m_upper + delta);
        NX_ASSERT(!result.isEmpty(), "Arithmetic overflow");
        return result;
    }

private:
    T m_lower = T();
    T m_upper = T();
};

} // namespace utils
} // namespace nx
