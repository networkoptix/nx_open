#pragma once

#include <limits>
#include <functional>
#include <type_traits>
#include <utility>

#include <QtCore/qnamespace.h>

namespace nx {
namespace utils {
namespace algorithm {

/**
* Merge sorted lists using priority queue concept implemented via STL heap.
*
* @param sortedLists List of source sorted lists. Its type should be random access iteratable.
*   Each list should be sequentially iteratable and have value_type type and push_back method.
* @param lessItem Item comparison functor taking two values of sorted list's value_type
*   and returning a boolean. Must be consistent with source lists sort order.
* @param totalLimit The maximum allowed number of items in the resulting list; ignored if <= 0.
* @returns Single merged sorted list.
*/
template<class ListOfSortedLists, class LessPredicate>
auto merge_sorted_lists(
    ListOfSortedLists sortedLists,
    LessPredicate lessItem,
    int totalLimit = std::numeric_limits<int>::max())
{
    using SortedList = typename std::remove_reference<decltype(*sortedLists.begin())>::type;

    switch (sortedLists.size())
    {
        case 0: return SortedList();
        case 1: return std::move(*sortedLists.begin());
        default: break;
    }

    using Iterator = typename SortedList::iterator;
    using IteratorRange = std::pair<Iterator, Iterator>;

    std::vector<IteratorRange> queueData;
    queueData.reserve(sortedLists.size());

    SortedList* lastNonEmptyList = nullptr;
    for (auto& list: sortedLists)
    {
        if (list.empty())
            continue;

        lastNonEmptyList = &list;
        queueData.push_back(std::make_pair(list.begin(), list.end()));
    }

    switch (queueData.size())
    {
        case 0: return SortedList();
        case 1: return std::move(*lastNonEmptyList);
        default: break;
    }

    SortedList result;
    static constexpr int kMaximumReserve = 10000; // Something sensible.

    if (totalLimit <= 0)
        totalLimit = std::numeric_limits<int>::max();

    result.reserve(std::min(totalLimit, kMaximumReserve));

    const auto lessPriority =
            [lessItem](const IteratorRange& left, const IteratorRange& right) -> bool
            {
                return !lessItem(*left.first, *right.first);
            };

    std::make_heap(queueData.begin(), queueData.end(), lessPriority);

    while (!queueData.empty() && ((int) result.size()) < totalLimit)
    {
        std::pop_heap(queueData.begin(), queueData.end(), lessPriority);
        auto& range = queueData.back();

        result.push_back(*range.first);
        ++range.first;

        if (range.first == range.second)
            queueData.pop_back();
        else
            std::push_heap(queueData.begin(), queueData.end(), lessPriority);
    }

    return std::move(result);
};

/**
* Merge sorted lists using priority queue concept implemented via STL heap.
*
* @param sortedLists List of source sorted lists. Its type should be random access iteratable.
*   Each list should be sequentially iteratable and have value_type type and push_back method.
* @param sortFieldGetter A functor taking value_type of a sorted list and returning a value
*   of type that can be compared with operator <.
* @param sortOrder How source sorted lists are sorted.
* @param totalLimit The maximum allowed number of items in the resulting list; ignored if <= 0.
* @returns Single merged sorted list.
*/
template<class ListOfSortedLists, class SortFieldGetter>
auto merge_sorted_lists(
    ListOfSortedLists sortedLists,
    SortFieldGetter sortFieldGetter,
    Qt::SortOrder sortOrder = Qt::AscendingOrder,
    int totalLimit = std::numeric_limits<int>::max())
{
    using SortedList = typename std::remove_reference<decltype(*sortedLists.begin())>::type;
    using Item = typename SortedList::value_type;

    if (sortOrder == Qt::AscendingOrder)
    {
        const auto less =
            [sortFieldGetter](const Item& left, const Item& right)
            {
                return sortFieldGetter(left) < sortFieldGetter(right);
            };

        return merge_sorted_lists(std::move(sortedLists), less, totalLimit);
    }

    const auto less =
        [sortFieldGetter](const Item& left, const Item& right)
        {
            return sortFieldGetter(left) > sortFieldGetter(right);
        };

    return merge_sorted_lists(std::move(sortedLists), less, totalLimit);
};

/**
* Merge sorted lists using priority queue concept implemented via STL heap.
*
* @param sortedLists List of source sorted lists. Its type should be random access iteratable.
*   Each list should be sequentially iteratable and have value_type type and push_back method.
*   Values should be comparable with operator <.
* @param sortOrder How source sorted lists are sorted.
* @param totalLimit The maximum allowed number of items in the resulting list; ignored if <= 0.
* @returns Single merged sorted list.
*/
template<class ListOfSortedLists>
auto merge_sorted_lists(
    ListOfSortedLists sortedLists,
    Qt::SortOrder sortOrder = Qt::AscendingOrder,
    int totalLimit = std::numeric_limits<int>::max())
{
    using SortedList = typename std::remove_reference<decltype(*sortedLists.begin())>::type;

    return merge_sorted_lists(std::move(sortedLists),
        [](const typename SortedList::value_type& value) { return value; },
        sortOrder, totalLimit);
}

} // namespace algorithm
} // namespace utils
} // namespace nx
