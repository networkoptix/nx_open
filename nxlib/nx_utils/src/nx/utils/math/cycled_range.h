#pragma once

#include <algorithm>
#include <nx/utils/math/arithmetic.h>
#include <nx/utils/std/optional.h>

namespace nx {
namespace utils {
namespace math {

template <typename Number>
class CycledRange
{
public:
    CycledRange() = default;

    CycledRange(Number position, Number lowerBound, Number upperBound):
        m_lowerBound(lowerBound),
        m_upperBound(upperBound),
        m_currentPosition(normalize(position))
    {
    };

    CycledRange<Number> operator+(Number other) const
    {
        const auto result = add(other);
        if (result == std::nullopt)
            return CycledRange();

        return CycledRange<Number>(result, m_lowerBound, m_upperBound);
    };

    CycledRange<Number> operator-(Number other) const
    {
        const auto result = add(-other);
        if (result == std::nullopt)
            return CycledRange();

        return CycledRange<Number>(result, m_lowerBound, m_upperBound);
    };

    CycledRange<Number>& operator+=(Number other)
    {
        m_currentPosition = add(other);
        return *this;
    };

    CycledRange<Number>& operator-=(Number other)
    {
        m_currentPosition = add(-other);
        return *this;
    };

    std::optional<Number> position() const
    {
        return m_currentPosition;
    }

private:
    std::optional<Number> add(Number other) const
    {
        if (m_currentPosition == std::nullopt)
            return std::nullopt;

        return normalize(*m_currentPosition + other);
    }

    std::optional<Number> normalize(Number position) const
    {
        if (position >= m_lowerBound && position <= m_upperBound)
            return position;

        const auto range = m_upperBound - m_lowerBound;
        if (range == 0)
            return m_lowerBound;

        const auto bound = position < m_lowerBound ? m_upperBound : m_lowerBound;
        const auto rem = math::remainder<Number>(position - bound, range);
        if (rem == std::nullopt)
            return std::nullopt;

        return std::clamp(bound + *rem, m_lowerBound, m_upperBound);
    }

private:
    Number m_lowerBound = Number();
    Number m_upperBound = Number();
    std::optional<Number> m_currentPosition{std::nullopt};
};

} // namespace math
} // namespace utils
} // namespace nx
