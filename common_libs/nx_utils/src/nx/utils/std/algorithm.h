#include <algorithm>
#include <map>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace nx {
namespace utils {

template<typename ForwardIt, typename T>
ForwardIt binary_find(ForwardIt first, ForwardIt last, const T& value)
{
    auto lbit = std::lower_bound(first, last, value);
    if (lbit != last && !(value < *lbit))
        return lbit;

    return last;
}

template<typename ForwardIt, typename T, typename Compare>
ForwardIt binary_find(ForwardIt first, ForwardIt last, const T& value, Compare comp)
{
    auto lbit = std::lower_bound(first, last, value, comp);
    if (lbit != last && !comp(value, *lbit))
        return lbit;

    return last;
}

/**
 * Elements from [first, last) range matching p are moved to outFirst.
 * And such element cell is moved to the end of [first, last) range.
 * @return Iterator to the end of new range without elements moved.
 */
template<typename InputIt, class OutputIt, typename UnaryPredicate>
InputIt move_if(InputIt first, InputIt last, OutputIt outFirst, UnaryPredicate p)
{
    using ValueType = decltype(*first);

    InputIt newRangeEndIt = std::stable_partition(
        first, last,
        [&p](const ValueType& elem)
        {
            return !p(elem);
        });
    for (auto it = newRangeEndIt; it != last; ++it)
        *outFirst++ = std::move(*it);

    return newRangeEndIt;
}

template<typename AssociativeContainer>
std::size_t countByPrefix(
    const AssociativeContainer& associativeContainer,
    typename const AssociativeContainer::key_type& prefix)
{
    std::size_t result = 0;
    for (auto it = associativeContainer.lower_bound(prefix);
        it != associativeContainer.end();
        ++it)
    {
        if (!boost::starts_with(it->first, prefix))
            break;
        ++result;
    }

    return result;
}

template<typename AssociativeContainer>
typename AssociativeContainer::iterator findAnyByPrefix(
    AssociativeContainer& associativeContainer,
    typename const AssociativeContainer::key_type& prefix)
{
    auto it = associativeContainer.lower_bound(prefix);
    if (it == associativeContainer.end() || !boost::starts_with(it->first, prefix))
        return associativeContainer.end();
    return it;
}

template<typename AssociativeContainer>
typename std::pair<
    typename AssociativeContainer::iterator,
    typename AssociativeContainer::iterator
>
    equalRangeByPrefix(
        AssociativeContainer& associativeContainer,
        typename const AssociativeContainer::key_type& prefix)
{
    typename std::pair<
        typename AssociativeContainer::iterator,
        typename AssociativeContainer::iterator
    > result;

    result.first = associativeContainer.lower_bound(prefix);
    result.second = result.first;
    for (auto it = result.first; it != associativeContainer.end(); ++it)
    {
        if (!boost::starts_with(it->first, prefix))
            break;
        result.second = it;
        ++result.second;
    }

    return result;
}

/**
 * E.g., given inbox.google.com returns com.google.inbox
 */
template<typename RandomAccessContainer, typename Separator>
RandomAccessContainer reverseWords(
    const RandomAccessContainer& str,
    const Separator& separator)
{
    std::vector<RandomAccessContainer> splitVec;
    boost::split(splitVec, str, boost::algorithm::is_any_of(separator));

    return boost::join(
        boost::make_iterator_range(splitVec.rbegin(), splitVec.rend()),
        separator);
}

} // namespace utils
} // namespace nx
