// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/Qt>

struct QnCameraBookmark;
class QnResourcePool;

namespace nx::vms::api { enum class BookmarkSortField; }

NX_VMS_COMMON_API std::function<bool(const QnCameraBookmark&, const QnCameraBookmark&)>
    createBookmarkSortPredicate(
        nx::vms::api::BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool);

namespace nx::vms::api {

struct Bookmark;

NX_VMS_COMMON_API std::function<bool(const Bookmark&, const Bookmark&)>
    createBookmarkSortPredicate(
        BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool);

} // namespace nx::vms::api
