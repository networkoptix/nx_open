// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <core/resource/camera_bookmark.h>
#include <nx/vms/api/data/bookmark_models.h>

class QnResourcePool;

namespace nx::vms::common {
/** Forward declaration. */
NX_VMS_COMMON_API std::function<bool(const CameraBookmark&, const CameraBookmark&)>
    createBookmarkSortPredicate(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool);

/** Forward declaration. */
NX_VMS_COMMON_API std::function<bool(const nx::vms::api::BookmarkV1&, const nx::vms::api::BookmarkV1&)>
    createBookmarkSortPredicateV1(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool);

/** Forward declaration. */
NX_VMS_COMMON_API std::function<bool(const nx::vms::api::BookmarkV3&, const nx::vms::api::BookmarkV3&)>
    createBookmarkSortPredicateV3(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool);

namespace detail {

template<typename BookmarkType>
inline BookmarkType createBookmarkAtTimePoint(
    const QnUuid& id, const std::chrono::milliseconds& time)
{
    BookmarkType result;
    result.setId(id);
    result.startTimeMs = time;
    return result;
}

template<>
inline CameraBookmark createBookmarkAtTimePoint<CameraBookmark>(
    const QnUuid& id, const std::chrono::milliseconds& time)
{
    CameraBookmark result;
    result.guid = id;
    result.startTimeMs = time;
    return result;
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
    return nx::vms::common::createBookmarkSortPredicate(sortField, sortOrder, resourcePool);
}

template<>
inline std::function<bool (const nx::vms::api::BookmarkV1&, const nx::vms::api::BookmarkV1&)>
    bookmarkSortPredicate<nx::vms::api::BookmarkV1>(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool)
{
    return nx::vms::common::createBookmarkSortPredicateV1(sortField, sortOrder, resourcePool);
}

template<>
inline std::function<bool(const nx::vms::api::BookmarkV3&, const nx::vms::api::BookmarkV3&)>
    bookmarkSortPredicate<nx::vms::api::BookmarkV3>(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool)
{
    return nx::vms::common::createBookmarkSortPredicateV3(sortField, sortOrder, resourcePool);
}

} // namespace detail

} // namespace nx::vms::common
