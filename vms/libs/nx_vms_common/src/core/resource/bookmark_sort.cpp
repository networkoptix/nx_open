// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_sort.h"

#include <algorithm>

#include <QtCore/QCoreApplication>

#include <api/helpers/camera_id_helper.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/vms/api/data/bookmark_models.h>
#include <utils/camera/bookmark_helpers.h>

using nx::vms::api::BookmarkSortField;

namespace {

struct BookmarkSort //< Just a context for translation functions.
{
    Q_DECLARE_TR_FUNCTIONS(BookmarkSort)
};

template<typename Bookmark>
struct BookmarkFacade
{
};

template<>
struct BookmarkFacade<QnCameraBookmark>: public BookmarkSort
{
    using Bookmark = QnCameraBookmark;

    static const auto& id(const Bookmark& bookmark) { return bookmark.guid; }
    static const auto& name(const Bookmark& bookmark) { return bookmark.name; }
    static auto description(const Bookmark& bookmark) { return bookmark.description; }
    static auto startTimeMs(const Bookmark& bookmark) { return bookmark.startTimeMs; }
    static auto durationMs(const Bookmark& bookmark) { return bookmark.durationMs; }

    static auto creationTimeMs(const Bookmark& bookmark)
    {
        return bookmark.creationTime().count();
    }

    static auto tags(const Bookmark& bookmark)
    {
        return std::set<QString>(bookmark.tags.cbegin(), bookmark.tags.cend());
    }

    static QString creatorName(const Bookmark& bookmark, QnResourcePool* resourcePool)
    {
        return helpers::getBookmarkCreatorName(bookmark, resourcePool);
    }

    static QString cameraName(const Bookmark& bookmark, QnResourcePool* resourcePool)
    {
        if (!NX_ASSERT(resourcePool))
            return {};

        const auto cameraResource = resourcePool->getResourceById<QnVirtualCameraResource>(
            bookmark.cameraId);

        return cameraResource
            ? QnResourceDisplayInfo(cameraResource).toString(Qn::RI_NameOnly)
            : nx::format("<%1>", tr("Removed camera")).toQString();
    }
};

template<>
struct BookmarkFacade<nx::vms::api::Bookmark>: public BookmarkSort
{
    using Bookmark = nx::vms::api::Bookmark;

    static const auto& id(const Bookmark& bookmark) { return bookmark.id; }
    static const auto& name(const Bookmark& bookmark) { return bookmark.name; }
    static auto description(const Bookmark& bookmark) { return bookmark.description; }
    static auto startTimeMs(const Bookmark& bookmark) { return bookmark.startTimeMs; }
    static auto durationMs(const Bookmark& bookmark) { return bookmark.durationMs; }

    static auto creationTimeMs(const Bookmark& bookmark) { return bookmark.creationTimeMs; }

    static const auto& tags(const Bookmark& bookmark)
    {
        return bookmark.tags;
    }

    static QString creatorName(const Bookmark& bookmark, QnResourcePool* resourcePool)
    {
        if (bookmark.creatorUserId.isNull())
            return QString();

        if (bookmark.creatorUserId == QnCameraBookmark::systemUserId())
            return tr("System Event", "Shows that the bookmark was created by a system event");

        const auto userResource = resourcePool->getResourceById<QnUserResource>(
            bookmark.creatorUserId);

        return userResource ? userResource->getName() : QString();
    }

    static QString cameraName(const Bookmark& bookmark, QnResourcePool* resourcePool)
    {
        if (!NX_ASSERT(resourcePool))
            return {};

        const auto cameraResource = nx::camera_id_helper::findCameraByFlexibleId(
            resourcePool, bookmark.deviceId);

        return cameraResource
            ? QnResourceDisplayInfo(cameraResource).toString(Qn::RI_NameOnly)
            : nx::format("<%1>", tr("Removed camera")).toQString();
    }
};

template<typename Bookmark, typename GetterFunc>
std::function<bool(const Bookmark&, const Bookmark&)>
    createSortPredicate(Qt::SortOrder sortOrder, GetterFunc getterFunc)
{
    const bool descending = sortOrder == Qt::DescendingOrder;

    const auto sortPredicate =
        [getterFunc, descending](const Bookmark& left, const Bookmark& right)
        {
            const auto l = getterFunc(left);
            const auto r = getterFunc(right);

            return descending ^ ((l != r)
                ? (l < r)
                : (BookmarkFacade<Bookmark>::id(left) < BookmarkFacade<Bookmark>::id(right)));
        };

    return sortPredicate;
}

template<typename Bookmark>
std::function<bool(const Bookmark&, const Bookmark&)>
    createGenericBookmarkSortPredicate(
        BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool)
{
    switch (sortField)
    {
        case BookmarkSortField::name:
            return createSortPredicate<Bookmark>(sortOrder, &BookmarkFacade<Bookmark>::name);

        case BookmarkSortField::description:
            return createSortPredicate<Bookmark>(sortOrder, &BookmarkFacade<Bookmark>::description);

        case BookmarkSortField::startTime:
            return createSortPredicate<Bookmark>(sortOrder, &BookmarkFacade<Bookmark>::startTimeMs);

        case BookmarkSortField::duration:
            return createSortPredicate<Bookmark>(sortOrder, &BookmarkFacade<Bookmark>::durationMs);

        case BookmarkSortField::creationTime:
            return createSortPredicate<Bookmark>(
                sortOrder, &BookmarkFacade<Bookmark>::creationTimeMs);

        case BookmarkSortField::tags:
            return createSortPredicate<Bookmark>(sortOrder, &BookmarkFacade<Bookmark>::tags);

        case BookmarkSortField::creator:
            return createSortPredicate<Bookmark>(sortOrder,
                [resourcePool](const Bookmark& bookmark)
                {
                    return BookmarkFacade<Bookmark>::creatorName(bookmark, resourcePool);
                });

        case BookmarkSortField::cameraName:
            return createSortPredicate<Bookmark>(sortOrder,
                [resourcePool](const Bookmark& bookmark)
                {
                    return BookmarkFacade<Bookmark>::cameraName(bookmark, resourcePool);
                });
    }

    NX_ASSERT(false, "Invalid bookmark sort field: '%1'", sortField);
    return createSortPredicate<Bookmark>(sortOrder, &BookmarkFacade<Bookmark>::id);
}

} // namespace

std::function<bool(const QnCameraBookmark&, const QnCameraBookmark&)>
    createBookmarkSortPredicate(
        BookmarkSortField sortField,
        Qt::SortOrder sortOrder,
        QnResourcePool* resourcePool)
{
    return createGenericBookmarkSortPredicate<QnCameraBookmark>(sortField, sortOrder, resourcePool);
}

namespace nx::vms::api {

std::function<bool(const Bookmark&, const Bookmark&)> createBookmarkSortPredicate(
    BookmarkSortField sortField,
    Qt::SortOrder sortOrder,
    QnResourcePool* resourcePool)
{
    return createGenericBookmarkSortPredicate<Bookmark>(sortField, sortOrder, resourcePool);
}

} // namespace nx::vms::api
