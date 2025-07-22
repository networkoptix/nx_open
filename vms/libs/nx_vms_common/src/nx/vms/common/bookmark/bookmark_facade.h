// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/bookmark_models.h>

namespace nx::vms::common {

struct NX_VMS_COMMON_API BookmarkFacadeStrings
{
    Q_DECLARE_TR_FUNCTIONS(BookmarkFacadeStrings)

protected:
    static QString creatorName(const std::optional<nx::Uuid>& creatorId, QnResourcePool* pool);
    static QString cameraName(const nx::Uuid& deviceId, QnResourcePool* pool);
    static QString siteEvent();
};

template<typename Bookmark>
struct BookmarkFacadeBase: protected BookmarkFacadeStrings
{
    using Type = Bookmark;
    using TimeType = std::chrono::milliseconds;

    static QString name(const Bookmark& bookmark)
    {
        return bookmark.name;
    }

    static QString description(const Bookmark& bookmark)
    {
        return bookmark.description;
    }

    static std::chrono::milliseconds startTime(const Bookmark& bookmark)
    {
        return bookmark.startTimeMs;
    }

    static std::chrono::milliseconds durationMs(const Bookmark& bookmark)
    {
        return bookmark.durationMs;
    }

    static bool equal(const Bookmark& left, const Bookmark& right)
    {
        return left == right;
    }
};

template<typename Bookmark>
struct BookmarkFacade: public BookmarkFacadeBase<Bookmark>
{
    using IdType = decltype(Bookmark::id);

    static auto id(const Bookmark& bookmark) { return bookmark.id; }
    static auto tags(const Bookmark& bookmark) { return bookmark.tags; }
    static auto deviceId(const Bookmark& bookmark) { return bookmark.deviceId; }
    static auto creatorUserId(const Bookmark& bookmark) { return bookmark.creatorUserId; }

    static std::chrono::milliseconds creationTimeMs(const Bookmark& bookmark)
    {
        return bookmark.creationTimeMs ? *bookmark.creationTimeMs : bookmark.startTimeMs;
    }

    static QString creatorName(const Bookmark& bookmark, QnResourcePool* pool)
    {
        return BookmarkFacadeStrings::creatorName(bookmark.creatorUserId, pool);
    }

    static QString cameraName(const Bookmark& bookmark, QnResourcePool* pool)
    {
        return BookmarkFacadeStrings::cameraName(bookmark.deviceId, pool);
    }
};

// Explicit instantiations, required for Windows DLL export.
template struct NX_VMS_COMMON_API BookmarkFacade<nx::vms::api::BookmarkV1>;
template struct NX_VMS_COMMON_API BookmarkFacade<nx::vms::api::BookmarkV3>;

template<>
struct NX_VMS_COMMON_API BookmarkFacade<QnCameraBookmark>:
    public BookmarkFacadeBase<QnCameraBookmark>
{
    using IdType = nx::Uuid;

    static auto id(const Type& bookmark) { return bookmark.guid; }
    static auto creationTimeMs(const Type& bookmark) { return bookmark.creationTimeStampMs; }
    static auto deviceId(const Type& bookmark) { return bookmark.cameraId; }
    static auto creatorUserId(const Type& bookmark) { return bookmark.creatorId; }

    static auto tags(const Type& bookmark)
    {
        return std::set<QString>(bookmark.tags.begin(), bookmark.tags.end());
    }

    static QString creatorName(const Type& bookmark, QnResourcePool* pool)
    {
        return BookmarkFacadeStrings::creatorName(bookmark.creatorId, pool);
    }

    static QString cameraName(const Type& bookmark, QnResourcePool* pool)
    {
        return BookmarkFacadeStrings::cameraName(bookmark.cameraId, pool);
    }
};

using QnBookmarkFacade = BookmarkFacade<QnCameraBookmark>;
using ApiBookmarkFacade = BookmarkFacade<nx::vms::api::Bookmark>;

} // namespace nx::vms::common
