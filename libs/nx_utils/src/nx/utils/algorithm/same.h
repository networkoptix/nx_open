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

} // namespace algorithm
} // namespace utils
} // namespace nx
