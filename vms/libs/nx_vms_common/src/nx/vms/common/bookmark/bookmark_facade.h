// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/bookmark_models.h>

#include "bookmark_helpers.h"

namespace nx::vms::common {

struct NX_VMS_COMMON_API BookmarkFacadeBase
{
    Q_DECLARE_TR_FUNCTIONS(BookmarkFacadeStrings)
protected:
    static QString creatorName(const QnUuid& creatorId,
        QnResourcePool* resourcePool);

    static QString cameraName(const QnUuid& cameraId, QnResourcePool* pool);
};

template<typename Bookmark>
struct NX_VMS_COMMON_API BookmarkFacade
{
};

template<>
struct BookmarkFacade<QnCameraBookmark>: protected BookmarkFacadeBase
{
    using Type = QnCameraBookmark;
    using TimeType = std::chrono::milliseconds;

    static const auto& id(const Type& bookmark) { return bookmark.guid; }
    static const auto& name(const Type& bookmark) { return bookmark.name; }
    static auto description(const Type& bookmark) { return bookmark.description; }
    static auto startTime(const Type& bookmark) { return bookmark.startTimeMs; }
    static auto durationMs(const Type& bookmark) { return bookmark.durationMs; }

    static auto creationTimeMs(const Type& bookmark)
    {
        return bookmark.creationTime().count();
    }

    static auto tags(const Type& bookmark)
    {
        return std::set<QString>(bookmark.tags.cbegin(), bookmark.tags.cend());
    }

    static QString creatorName(
        const Type& bookmark,
        QnResourcePool* resourcePool)
    {
        return BookmarkFacadeBase::creatorName(bookmark.creatorId, resourcePool);
    }

    static QString cameraName(const Type& bookmark, QnResourcePool* resourcePool)
    {
        return BookmarkFacadeBase::cameraName(bookmark.cameraId, resourcePool);
    }

    static bool equal(const Type& left, const Type& right)
    {
        return left == right;
    }
};

template<>
struct NX_VMS_COMMON_API BookmarkFacade<nx::vms::api::Bookmark>: protected BookmarkFacadeBase
{
    using Type = nx::vms::api::Bookmark;
    using TimeType = std::chrono::milliseconds;

    static const auto& id(const Type& bookmark) { return bookmark.id; }
    static const auto& name(const Type& bookmark) { return bookmark.name; }
    static auto description(const Type& bookmark) { return bookmark.description; }
    static auto startTime(const Type& bookmark) { return bookmark.startTimeMs; }
    static auto durationMs(const Type& bookmark) { return bookmark.durationMs; }

    static auto creationTimeMs(const Type& bookmark) { return bookmark.creationTimeMs; }

    static const auto& tags(const Type& bookmark)
    {
        return bookmark.tags;
    }

    static QString creatorName(
        const Type& bookmark,
        QnResourcePool* resourcePool)
    {
        return bookmark.creatorUserId
            ? BookmarkFacadeBase::creatorName(*bookmark.creatorUserId, resourcePool)
            : QString{};
    }

    static QString cameraName(const Type& bookmark, QnResourcePool* resourcePool)
    {
        return BookmarkFacadeBase::cameraName(
            QnUuid::fromStringSafe(bookmark.deviceId), resourcePool);
    }

    static bool equal(const Type& left, const Type& right)
    {
        return left == right;
    }
};

using QnBookmarkFacade = BookmarkFacade<QnCameraBookmark>;
using ApiBookmarkFacade = BookmarkFacade<nx::vms::api::Bookmark>;

} // namespace nx::vms::common
