// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cmath>

#include <nx/reflect/generic_visitor.h>
#include <nx/reflect/instrument.h>

namespace nx::reflect {

namespace detail {

static inline bool fuzzyIsNull(double d)
{
    return std::fabs(d) <= 0.000'000'000'001;
}

static inline bool fuzzyIsNull(float f)
{
    return std::fabs(f) <= 0.000'01f;
}

static inline bool fuzzyEquals(double p1, double p2)
{
    constexpr double kPrecision = 1'000'000'000'000.0;
    return (fuzzyIsNull(p1) && fuzzyIsNull(p2)) ||
        (std::fabs(p1 - p2) * kPrecision <= std::fmin(std::fabs(p1), std::fabs(p2)));
}

static inline bool fuzzyEquals(float p1, float p2)
{
    constexpr float kPrecision = 100'000.0f;
    return (fuzzyIsNull(p1) && fuzzyIsNull(p2)) ||
        (std::fabs(p1 - p2) * kPrecision <= std::fmin(std::fabs(p1), std::fabs(p2)));
}

template<typename T>
class EqualityEnumerator:
    public nx::reflect::GenericVisitor<EqualityEnumerator<T>>
{
public:
    EqualityEnumerator(const T& left, const T& right):
        m_left(left),
        m_right(right)
    {
    }

    template<typename WrappedField>
    void visitField(const WrappedField& field)
    {
        if constexpr (std::is_floating_point_v<typename WrappedField::Type>)
        {
            m_equals &= fuzzyEquals(field.get(m_left), field.get(m_right));
        }
        else
        {
            m_equals &= (field.get(m_left) == field.get(m_right));
        }
    }

    bool finish()
    {
        return m_equals;
    }

private:
    const T& m_left;
    const T& m_right;
    bool m_equals = true;
};

} // namespace detail

/**
 * Equally-compares two objects of the same type by iterating their members instrumented with
 * nx::reflect. Compares with precision up to the 6th digit for floats and 12th digit for doubles.
 * TODO: It makes sense to refactor this to 3way comparison.
 */
template<typename T>
bool equals(const T& left, const T& right)
{
    detail::EqualityEnumerator<T> equalityEnumerator(left, right);
    return nx::reflect::visitAllFields<T>(equalityEnumerator);
}

} // namespace nx::reflect
