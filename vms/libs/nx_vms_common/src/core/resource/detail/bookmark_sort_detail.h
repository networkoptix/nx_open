// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource/camera_bookmark.h>
#include <nx/vms/api/data/bookmark_models.h>

class QnResourcePool;

/** Forward declaration. */
NX_VMS_COMMON_API std::function<bool(const QnCameraBookmark&, const QnCameraBookmark&)>
    createBookmarkSortPredicate(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool);

namespace nx::vms::common {

/** Forward declaration. */
NX_VMS_COMMON_API std::function<bool(const nx::vms::api::Bookmark&, const nx::vms::api::Bookmark&)>
    createBookmarkSortPredicate(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool);

namespace detail {

template<typename BookmarkType>
inline BookmarkType createBookmarkAtTimePoint(
    const QnUuid& id,
    const std::chrono::milliseconds& time)
{
    BookmarkType result;
    result.guid = id;
    result.startTimeMs = time;
    return result;
}

template<>
inline nx::vms::api::Bookmark createBookmarkAtTimePoint<nx::vms::api::Bookmark>(
    const QnUuid& id, const std::chrono::milliseconds& time)
{
    return nx::vms::api::Bookmark{.id = id, .startTimeMs = time};
}

//-------------------------------------------------------------------------------------------------

template<typename Value>
struct ValueExtractor
{
    using value_type = Value;
    const Value& operator()(const Value& value) const
    {
        return value;
    }
};

template<typename Key, typename Value>
struct ValueExtractor<std::pair<Key, Value>>
{
    using value_type = Value;
    const Value& operator()(const std::pair<Key, Value>& value) const
    {
        return value.second;
    }
};

//-------------------------------------------------------------------------------------------------

template<typename BookmarkType>
inline std::function<bool (const BookmarkType&, const BookmarkType&)> bookmarkSortPredicate(
    nx::vms::api::BookmarkSortField sortField,
    Qt::SortOrder sortOrder,
    QnResourcePool* resourcePool)
{
    return ::createBookmarkSortPredicate(sortField, sortOrder, resourcePool);
}

template<>
inline std::function<bool (const nx::vms::api::Bookmark&, const nx::vms::api::Bookmark&)>
    bookmarkSortPredicate<nx::vms::api::Bookmark>(
    nx::vms::api::BookmarkSortField sortField,
    Qt::SortOrder sortOrder,
    QnResourcePool* resourcePool)
{
    return nx::vms::common::createBookmarkSortPredicate(sortField, sortOrder, resourcePool);
}

} // namespace detail

} // namespace nx::vms::common
