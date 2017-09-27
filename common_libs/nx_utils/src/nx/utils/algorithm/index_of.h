#pragma once

#include <iterator>

namespace nx {
namespace utils {
namespace algorithm {

/** Return index of an element in range. */
template<class Iterator, class Predicate> inline
int index_of(Iterator first, Iterator last, Predicate pred)
{
    for (auto iter = first; iter != last; ++iter)
    {
        if (pred(*iter))
            return std::distance(first, iter);
    }
    return -1;
}

template<class Range, class Predicate> inline
int index_of(Range range, Predicate pred)
{
    return index_of(std::begin(range), std::end(range), pred);
}

} // namespace algorithm
} // namespace utils
} // namespace nx