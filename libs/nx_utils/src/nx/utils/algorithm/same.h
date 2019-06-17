#pragma once

namespace nx {
namespace utils {
namespace algorithm {

/** Test if predicate returns the same value for all elements. */
template<class Iterator, class Predicate, class Elem> inline
bool same(Iterator first, Iterator last, Predicate pred, Elem* data = nullptr)
{
    if (first == last)
        return true;

    const auto firstValue = pred(*first);
    if (data)
        *data = firstValue;

    ++first;
    if (first == last)
        return true;

    for (; first != last; ++first)
        if (pred(*first) != firstValue)
            return false;

    return true;
}

//-------------------------------------------------------------------------------------------------

/**
 * Checks that two sorted ranges contain elements of same value using comparator comp.
 * Though, different element count is allowed.
 * NOTE: Multiple same elements in either range are allowed.
 * @param Comp comparator used to sort ranges.
 */
template<class FirstRangeIter, class SecondRangeIter, class Comp>
bool same_sorted_ranges_if(
    const FirstRangeIter& firstRangeBegin, const FirstRangeIter& firstRangeEnd,
    const SecondRangeIter& secondRangeBegin, const SecondRangeIter& secondRangeEnd,
    Comp comp)
{
    FirstRangeIter fIt = firstRangeBegin;
    SecondRangeIter sIt = secondRangeBegin;
    for (; (fIt != firstRangeEnd) || (sIt != secondRangeEnd); )
    {
        if (((fIt == firstRangeEnd) != (sIt == secondRangeEnd)) ||
            (comp(*fIt, *sIt) || comp(*sIt, *fIt)))    //*fIt != *sIt
        {
            return false;
        }

        ++fIt;
        //iterating second range while *sIt < *fIt
        const auto& prevSVal = *sIt;
        while ((sIt != secondRangeEnd) &&
            ((sIt == secondRangeEnd) < (fIt == firstRangeEnd) || comp(*sIt, *fIt)))
        {
            if (comp(prevSVal, *sIt) || comp(*sIt, prevSVal))   //prevSVal != *sIt
                return false;   //second range element changed value but still less than first range element
            ++sIt;
        }
    }

    return true;
}

/**
 * Checks if sorted ranges contain same elements.
 * NOTE: Multiple same elements in either range are allowed.
 */
template<class FirstRangeIter, class SecondRangeIter>
bool same_sorted_ranges(
    const FirstRangeIter& firstRangeBegin, const FirstRangeIter& firstRangeEnd,
    const SecondRangeIter& secondRangeBegin, const SecondRangeIter& secondRangeEnd)
{
    return same_sorted_ranges_if(
        firstRangeBegin, firstRangeEnd,
        secondRangeBegin, secondRangeEnd,
        [](const auto& left, const auto& right) { return left < right; });
}

} // namespace algorithm
} // namespace utils
} // namespace nx
