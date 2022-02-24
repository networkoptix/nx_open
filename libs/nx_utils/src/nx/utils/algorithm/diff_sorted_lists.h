// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

namespace nx::utils::algorithm {

/**
 * Finds difference between two sorted lists: left and right.
 * Elements present in left but not right are passed to leftOnlyFunc.
 * Elements present in right but not left are passed to rightOnlyFunc.
 * Equal elements (i.e., intersection of two lists) are passed to equalFunc as equalFunc(left, right).
 */
template<
    typename It1, typename It2,
    typename LeftOnlyFunc, typename RightOnlyFunc,
    typename EqualFunc,
    typename Compare
>
void full_difference(
    const It1& leftBegin, const It1& leftEnd,
    const It2& rightBegin, const It2& rightEnd,
    LeftOnlyFunc leftOnlyFunc,
    RightOnlyFunc rightOnlyFunc,
    EqualFunc equalFunc,
    Compare comp)
{
    auto it1 = leftBegin;
    auto it2 = rightBegin;
    while (it1 != leftEnd || it2 != rightEnd)
    {
        if (it1 == leftEnd)
        {
            rightOnlyFunc(*it2);
            ++it2;
        }
        else if (it2 == rightEnd)
        {
            leftOnlyFunc(*it1);
            ++it1;
        }
        else
        {
            if (comp(*it1, *it2))
            {
                leftOnlyFunc(*it1);
                ++it1;
            }
            else if (comp(*it2, *it1))
            {
                rightOnlyFunc(*it2);
                ++it2;
            }
            else
            {
                equalFunc(*it1, *it2);
                ++it1;
                ++it2;
            }
        }
    }
}

/**
 * Same as above but uses operator< as a comparator.
 */
template<
    typename It1, typename It2,
    typename LeftOnlyFunc, typename RightOnlyFunc,
    typename EqualFunc
>
void full_difference(
    const It1& leftBegin, const It1& leftEnd,
    const It2& rightBegin, const It2& rightEnd,
    LeftOnlyFunc leftOnlyFunc,
    RightOnlyFunc rightOnlyFunc,
    EqualFunc equalFunc)
{
    full_difference(
        leftBegin, leftEnd, rightBegin, rightEnd,
        std::move(leftOnlyFunc), std::move(rightOnlyFunc), std::move(equalFunc),
        [](const auto& left, const auto& right) { return left < right; });
}

/**
 * Analyzes two sorted lists: left and right.
 * Elements found in left but not right are passed to leftOnlyOutIt.
 * Elements found in right but not left are passed to rightOnlyOutIt.
 * Equal elements (i.e., intersection of lists) are passed to equalOutIt.
 * To be more specific, `*equalOutIt=*leftIter` is invoked.
 */
template<
    typename It1, typename It2,
    typename LeftOnlyOutIt, typename RightOnlyOutIt,
    typename EqualOutIt,
    typename Compare
>
void full_difference_2(
    const It1& leftBegin, const It1& leftEnd,
    const It2& rightBegin, const It2& rightEnd,
    LeftOnlyOutIt leftOnlyOutIt,
    RightOnlyOutIt rightOnlyOutIt,
    EqualOutIt equalOutIt,
    Compare comp)
{
    full_difference(
        leftBegin, leftEnd, rightBegin, rightEnd,
        [&leftOnlyOutIt](const auto& val) { *leftOnlyOutIt = val; },
        [&rightOnlyOutIt](const auto& val) { *rightOnlyOutIt = val; },
        [&equalOutIt](const auto& left, const auto& /*right*/) { *equalOutIt = left; },
        comp);
}

/**
 * Same as above but uses operator< for comparison.
 */
template<
    typename It1, typename It2,
    typename LeftOnlyOutIt, typename RightOnlyOutIt,
    typename EqualOutIt
>
void full_difference_2(
    const It1& leftBegin, const It1& leftEnd,
    const It2& rightBegin, const It2& rightEnd,
    LeftOnlyOutIt leftOnlyOutIt,
    RightOnlyOutIt rightOnlyOutIt,
    EqualOutIt equalOutIt)
{
    full_difference_2(
        leftBegin, leftEnd, rightBegin, rightEnd,
        leftOnlyOutIt, rightOnlyOutIt, equalOutIt,
        [](const auto& left, const auto& right) { return left < right; });
}

} // namespace nx::utils::algorithm
