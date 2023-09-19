// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include "details/accessor.h"

namespace nx::utils::algorithm {

/*
 * Predicate which compares according to the specified order recurrently by the values from the
 * list of accessors.
 */
template<typename Accessor, typename... Accessors>
class Comparator
{
public:
    using Type = typename detail::Accessor<Accessor>::Type;
    using ValueType = typename detail::Accessor<Accessor>::ValueType;

    Comparator(bool ascending, const Accessor& accessor, Accessors ...accessors):
        m_accessor(accessor),
        m_pred(ascending
            ? BinaryPred(std::less<ValueType>())
            : BinaryPred(std::greater<ValueType>())),
        m_nested(ascending, std::forward<Accessors>(accessors)...)
    {
    }

    bool operator()(const Type& left, const Type& right) const
    {
        const ValueType& leftValue = m_accessor(left);
        const ValueType& rightValue = m_accessor(right);

        const bool predResult = m_pred(leftValue, rightValue);
        return !predResult && ! m_pred(rightValue, leftValue) //< Means equal values.
            ? m_nested(left, right)
            : predResult;
    }

private:
    using BinaryPred = std::function<bool (const ValueType& left, const ValueType& right)>;
    const detail::Accessor<Accessor> m_accessor;
    const BinaryPred m_pred;
    const Comparator<Accessors...> m_nested;
};

template<typename Accessor>
class Comparator<Accessor>
{
public:
    using Type = typename detail::Accessor<Accessor>::Type;
    using ValueType = typename detail::Accessor<Accessor>::ValueType;

    Comparator(bool ascending, const Accessor& accessor):
        m_accessor(accessor),
        m_pred(ascending
            ? BinaryPred(std::less<ValueType>{})
            : BinaryPred(std::greater<ValueType>{}))
    {
    }

    bool operator()(const Type& left, const Type& right) const
    {
        const ValueType& leftValue = m_accessor(left);
        const ValueType& rightValue = m_accessor(right);
        return m_pred(leftValue, rightValue);
    }

private:
    using BinaryPred = std::function<bool (const ValueType& left, const ValueType& right)>;
    const detail::Accessor<Accessor> m_accessor;
    const BinaryPred m_pred;
};

} // namespace nx::utils::algorithm
