#pragma once

#include <functional>
#include <type_traits>
#include <utility>

#include <QtCore/Qt>

namespace nx::utils::algorithm {

/**
 * Virtually merge sorted lists until specified number of items is processed,
 * then truncate source lists in the way that all off-limit items are erased.
 * Merge is done using priority queue concept implemented via STL heap.
 *
 * @param sortedLists List of source sorted lists or pointers to them.
 *   Its type should be random access iteratable.
 *   Each list should be sequentially iteratable and have value_type type,
 *   size and erase(first, last) methods.
 * @param totalLimit The maximum allowed number of items in virtual merged list.
 * @param lessItem Item comparison functor taking two values of sorted list's value_type
 *   and returning a boolean. Must be consistent with source lists sort order.
 * @returns Total number of items in truncated lists.
 */
template<typename ListOfSortedLists, typename LessPredicate>
int truncate_sorted_lists(
    ListOfSortedLists&& sortedLists,
    int totalLimit,
    LessPredicate lessItem)
{
    using SortedListsItem = std::remove_reference_t<decltype(*sortedLists.begin())>;
    using SortedList = std::remove_pointer_t<SortedListsItem>;

    const auto deref =
        [](auto&& item) -> auto&
        {
            if constexpr (std::is_pointer_v<std::remove_reference_t<decltype(item)>>)
                return *item;
            else
                return item;
        };

    if (totalLimit <= 0)
    {
        for (auto& list: sortedLists)
            deref(list) = SortedList();

        return 0;
    }

    const auto truncateList =
        [](SortedList& list, int limit) -> int
        {
            if ((int) list.size() <= limit)
                return (int) list.size();

            auto outOfLimit = list.begin();
            std::advance(outOfLimit, limit);
            list.erase(outOfLimit, list.end());
            return limit;
        };

    switch (sortedLists.size())
    {
        case 0: return 0;
        case 1: return truncateList(deref(*sortedLists.begin()), totalLimit);
        default: break;
    }

    using Iterator = typename SortedList::iterator;
    using CurrentPosition = std::pair<Iterator, SortedList*>;

    std::vector<CurrentPosition> queueData;
    queueData.reserve(sortedLists.size());

    SortedList* lastNonEmptyList = nullptr;
    int sourceCount = 0;

    for (auto& sortedListsItem: sortedLists)
    {
        auto& list = deref(sortedListsItem);
        if (list.empty())
            continue;

        lastNonEmptyList = &deref(list);
        sourceCount += (int) lastNonEmptyList->size();
        queueData.push_back(std::make_pair(lastNonEmptyList->begin(), lastNonEmptyList));
    }

    if (sourceCount < totalLimit)
        return sourceCount;

    switch (queueData.size())
    {
        case 0: return 0;
        case 1: return truncateList(*lastNonEmptyList, totalLimit);
        default: break;
    }

    const auto lessPriority =
        [lessItem](const CurrentPosition& left, const CurrentPosition& right) -> bool
        {
            return !lessItem(*left.first, *right.first);
        };

    std::make_heap(queueData.begin(), queueData.end(), lessPriority);

    int totalCount = 0;
    while (!queueData.empty() && totalCount < totalLimit)
    {
        std::pop_heap(queueData.begin(), queueData.end(), lessPriority);
        auto& position = queueData.back();

        ++totalCount;
        ++position.first;

        if (position.first == position.second->end())
            queueData.pop_back();
        else
            std::push_heap(queueData.begin(), queueData.end(), lessPriority);
    }

    for (auto& pos: queueData)
        pos.second->erase(pos.first, pos.second->end());

    return totalCount;
};

/**
 * Virtually merge sorted lists until specified number of items is processed,
 * then truncate source lists in the way that all off-limit items are erased.
 * Merge is done using priority queue concept implemented via STL heap.
 *
 * @param sortedLists List of source sorted lists or pointers to them.
 *   Its type should be random access iteratable.
 *   Each list should be sequentially iteratable and have value_type type,
 *   size and erase(first, last) methods.
 * @param sortFieldGetter A functor taking value_type of a sorted list and returning a value
 *   of type that can be compared with operator <.
 * @param totalLimit The maximum allowed number of items in virtual merged list.
 * @param sortOrder How source sorted lists are sorted.
 * @returns Total number of items in truncated lists.
 */
template<typename ListOfSortedLists, typename SortFieldGetter>
int truncate_sorted_lists(
    ListOfSortedLists&& sortedLists,
    SortFieldGetter sortFieldGetter,
    int totalLimit,
    Qt::SortOrder sortOrder = Qt::AscendingOrder)
{
    using SortedList = std::remove_pointer_t<std::remove_reference_t<decltype(*sortedLists.begin())>>;
    using Item = typename SortedList::value_type;

    using Less = std::function<bool(const Item&, const Item&)>;
    const auto less = sortOrder == Qt::AscendingOrder
        ? Less([key = sortFieldGetter](const Item& l, const Item& r) { return key(l) < key(r); })
        : Less([key = sortFieldGetter](const Item& l, const Item& r) { return key(l) > key(r); });

    return truncate_sorted_lists(std::forward<ListOfSortedLists>(sortedLists), totalLimit, less);
};

/**
 * Virtually merge sorted lists until specified number of items is processed,
 * then truncate source lists in the way that all off-limit items are erased.
 * Merge is done using priority queue concept implemented via STL heap.
 *
 * @param sortedLists List of source sorted lists or pointers to them.
 *   Its type should be random access iteratable.
 *   Each list should be sequentially iteratable and have value_type type,
 *   size and erase(first, last) methods.
 *   Values should be comparable with operator<
 * @param totalLimit The maximum allowed number of items in virtual merged list.
 * @param sortOrder How source sorted lists are sorted.
 * @returns Total number of items in truncated lists.
 */
template<typename ListOfSortedLists>
int truncate_sorted_lists(
    ListOfSortedLists&& sortedLists,
    int totalLimit,
    Qt::SortOrder sortOrder = Qt::AscendingOrder)
{
    using SortedList = std::remove_pointer_t<std::remove_reference_t<decltype(*sortedLists.begin())>>;

    return truncate_sorted_lists(std::forward<ListOfSortedLists>(sortedLists),
        [](const typename SortedList::value_type& value) { return value; },
        totalLimit, sortOrder);
}

} // namespace nx::utils::algorithm
