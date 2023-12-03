// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmarks_manager.h"

#include <camera/camera_bookmarks_query.h>
#include <camera/private/camera_bookmarks_manager_p.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/common/bookmark/bookmark_facade.h>

QnCameraBookmarksManager::QnCameraBookmarksManager(
    nx::vms::client::core::SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    d(new QnCameraBookmarksManagerPrivate(systemContext))
{
}

QnCameraBookmarksManager::~QnCameraBookmarksManager()
{
}

int QnCameraBookmarksManager::getBookmarksAsync(const QnCameraBookmarkSearchFilter& filter,
    BookmarksCallbackType callback)
{
    return d->getBookmarksAsync(filter, callback);
}

int QnCameraBookmarksManager::getBookmarksAroundPointAsync(
    const QnCameraBookmarkSearchFilter& filter,
    const QnCameraBookmarkList& source,
    BookmarksAroundPointCallbackType&& bookmarkCallback)
{
    if (!NX_ASSERT(filter.centralTimePointMs))
        return 0;

    NX_ASSERT(filter.orderBy.column == nx::vms::api::BookmarkSortField::startTime);

    static const auto kCentralPointSupportApiVersion = nx::utils::SoftwareVersion(6, 0);
    if (d->systemContext()->moduleInformation().version < kCentralPointSupportApiVersion)
    {
        // Emulate request around the point using a usual bookmarks request.
        return d->getBookmarkstAroundPointHeuristic(filter, source, /*triesCount*/ 10, bookmarkCallback);
    }

    auto bookmarksCallback = BookmarksCallbackType(
        [callback = std::move(bookmarkCallback), source, filter]
            (bool success, int id, QnCameraBookmarkList bookmarks)
        {
            using namespace nx::vms::client::core;
            const auto direction =  EventSearch::directionFromSortOrder(filter.orderBy.order);
            auto fetched = makeFetchedData<nx::vms::common::QnBookmarkFacade>(
                source, bookmarks, FetchRequest{direction, *filter.centralTimePointMs});
            callback(success, id, std::move(fetched));
        });

    return getBookmarksAsync(filter, std::move(bookmarksCallback));
}

int QnCameraBookmarksManager::getBookmarkTagsAsync(int maxTags, BookmarkTagsCallbackType callback)
{
    return d->getBookmarkTagsAsync(maxTags, callback);
}

void QnCameraBookmarksManager::addCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark");
    d->addCameraBookmark(bookmark, callback);

    emit bookmarkAdded(bookmark);
}

void QnCameraBookmarksManager::addAcknowledge(
    const QnCameraBookmark &bookmark,
    const QnUuid& eventRuleId,
    OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark");
    d->acknowledgeEvent(bookmark, eventRuleId, callback);

    emit bookmarkAdded(bookmark);
}

void QnCameraBookmarksManager::updateCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark");
    d->updateCameraBookmark(bookmark, callback);

    emit bookmarkUpdated(bookmark);
}

void QnCameraBookmarksManager::deleteCameraBookmark(const QnUuid &bookmarkId, OperationCallbackType callback)
{
    d->deleteCameraBookmark(bookmarkId, callback);

    emit bookmarkRemoved(bookmarkId);
}

void QnCameraBookmarksManager::sendQueryRequest(
    const QnCameraBookmarksQueryPtr& query,
    BookmarksCallbackType callback)
{
    NX_ASSERT(query);
    d->sendQueryRequest(query, callback);
}

void QnCameraBookmarksManager::sendQueryTailRequest(
    const QnCameraBookmarksQueryPtr& query,
    std::chrono::milliseconds timePoint,
    BookmarksCallbackType callback)
{
    d->sendQueryTailRequest(query, timePoint, callback);
}

QnCameraBookmarksQueryPtr QnCameraBookmarksManager::createQuery(const QnCameraBookmarkSearchFilter& filter)
{
    return d->createQuery(filter);
}
