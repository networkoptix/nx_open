// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/Qt>

#include <nx/utils/algorithm/merge_sorted_lists.h>

#include "detail/bookmark_sort_detail.h"

NX_VMS_COMMON_API std::function<bool(const QnCameraBookmark&, const QnCameraBookmark&)>
    createBookmarkSortPredicate(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool);

namespace nx::vms::common {

NX_VMS_COMMON_API std::function<bool(const nx::vms::api::BookmarkV1&, const nx::vms::api::BookmarkV1&)>
    createBookmarkSortPredicateV1(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool);

NX_VMS_COMMON_API std::function<bool(const nx::vms::api::BookmarkV3&, const nx::vms::api::BookmarkV3&)>
    createBookmarkSortPredicateV3(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool);

/** Sort bookmarks with the specified field and order. */
template<typename Bookmarks>
void sortBookmarks(
    Bookmarks& bookmarks,
    nx::vms::api::BookmarkSortField field,
    Qt::SortOrder order,
    QnResourcePool* resourcePool)
{
    using BookmarkType = typename Bookmarks::value_type;
    const auto pred = detail::bookmarkSortPredicate<BookmarkType>(field, order, resourcePool);
    std::sort(bookmarks.begin(), bookmarks.end(), pred);
}

/** Merge sorted by start time and uuid bookmarks lists around specified time point. */
template<typename BookmarksContainer>
typename detail::ValueExtractor<typename BookmarksContainer::value_type>::value_type
    mergeSortedBookmarksAroundTimePoint(
        const BookmarksContainer& serverBookmarks,
        const std::chrono::milliseconds& centralTimePointMs,
        Qt::SortOrder order,
        std::optional<int> limit,
        QnResourcePool* resourcePool)
{
    using ContainerValueType =
        typename BookmarksContainer::value_type; //< Container or pair<Key, Container>.
    using BookmarkList =
        typename detail::ValueExtractor<ContainerValueType>::value_type; //< Container type.
    using BookmarkMultipleLists = std::vector<BookmarkList>; //< Vector of bookmark containers.
    using BookmarkType = typename BookmarkList::value_type; //< Bookmark type.

    BookmarkMultipleLists bodies, tails;
    bodies.reserve(serverBookmarks.size());
    tails.reserve(serverBookmarks.size());

    const bool ascending = order == Qt::AscendingOrder;
    const auto pred = detail::bookmarkSortPredicate<BookmarkType>(
        nx::vms::api::BookmarkSortField::startTime, order, resourcePool);

    const auto edgeUuid = ascending
        ? QnUuid::fromString("{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}") //< Maximum value.
        : QnUuid(); //< Minimum value.

    const detail::ValueExtractor<ContainerValueType> valueExtractor;
    const auto splitBookmark = detail::createBookmarkAtTimePoint<BookmarkType>(
        edgeUuid, centralTimePointMs);
    for (const auto& value: serverBookmarks)
    {
        const auto& bookmarks = valueExtractor(value);
        const auto bodyEndIt = std::lower_bound(bookmarks.cbegin(), bookmarks.cend(),
            splitBookmark, pred);
        bodies.push_back(BookmarkList{bookmarks.cbegin(), bodyEndIt});
        tails.push_back(BookmarkList{bodyEndIt, bookmarks.cend()});
    }

    BookmarkList body = nx::utils::algorithm::merge_sorted_lists(
        std::move(bodies), pred, std::numeric_limits<int>::max());

    // We cut front body part since we want to save more bookmarks around the reference point.
    const int sideLimit = limit.value_or(std::numeric_limits<int>::max()) / 2;
    if (body.size() > sideLimit)
        body.erase(body.begin(), body.begin() + (body.size() - sideLimit));

    BookmarkList tail = nx::utils::algorithm::merge_sorted_lists(
        std::move(tails), pred, sideLimit); //< It is ok to cut the tail from the back.

    BookmarkMultipleLists result{std::move(tail), std::move(body)};
    return nx::utils::algorithm::merge_sorted_lists(
        std::move(result), pred, std::numeric_limits<int>::max());
}

} // namespace nx::vms::common
