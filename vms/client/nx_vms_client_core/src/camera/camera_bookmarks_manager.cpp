// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmarks_manager.h"

#include <camera/camera_bookmarks_query.h>
#include <camera/private/camera_bookmarks_manager_p.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/log_strings_format.h>
#include <nx/vms/common/api/helpers/bookmark_api_converter.h>
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

void QnCameraBookmarksManager::addExistingBookmark(const QnCameraBookmark& bookmark)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark");
    d->addExistingBookmark(bookmark);

    emit bookmarkAdded(bookmark);
}

bool QnCameraBookmarksManager::changeBookmarkRest(BookmarkOperation operation,
    const QnCameraBookmark& bookmark,
    OperationV4Callback&& callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark must not be added");
    if (!bookmark.isValid())
        return false;

    const auto serverId = d->getServerForBookmark(bookmark);
    if (!serverId)
        return false;

    const auto model = nx::vms::common::bookmarkToApi<nx::vms::api::BookmarkV3>(
        bookmark, *serverId, /*includeDigest*/ true);
    const auto path = operation == BookmarkOperation::create
        ? nx::format("rest/v4/devices/%1/bookmarks", model.deviceId).toQString()
        : nx::format("rest/v4/devices/%1/bookmarks/%2", model.deviceId, model.id).toQString();
    const auto method = operation == BookmarkOperation::create
        ? nx::network::http::Method::post
        : nx::network::http::Method::patch;

    auto localCallback =
        [this, requestCallback = std::move(callback), operation]
            (bool success, rest::Handle /*handle*/, rest::ErrorOrData<QByteArray> response)
        {
            const auto errorMessage = operation == BookmarkOperation::create
                ? "Cannot create bookmark."
                : "Cannot update bookmark.";
            NX_LOG_RESPONSE(this, success, response, errorMessage);

            nx::vms::api::BookmarkV3 result;
            if (!success || !response.has_value()
                || !QJson::deserialize<nx::vms::api::BookmarkV3>(response.value(), &result))
            {
                requestCallback(false, {});
                return;
            }

            requestCallback(true, result);

            const auto bookmark = nx::vms::common::bookmarkFromApi(result); //< result is moved here.
            if (operation == BookmarkOperation::create)
                emit bookmarkAdded(bookmark);
            else
                emit bookmarkUpdated(bookmark);
        };

    return d->sendRestRequest(path, model, method, std::move(localCallback)) != -1;
}

void QnCameraBookmarksManager::addAcknowledge(
    const QnCameraBookmark &bookmark,
    const nx::Uuid& eventRuleId,
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

void QnCameraBookmarksManager::deleteCameraBookmark(const nx::Uuid &bookmarkId, OperationCallbackType callback)
{
    d->deleteCameraBookmark(bookmarkId, callback);

    emit bookmarkRemoved(bookmarkId);
}

rest::Handle QnCameraBookmarksManager::sendQueryRequest(
    const QnCameraBookmarksQueryPtr& query,
    BookmarksCallbackType callback)
{
    NX_ASSERT(query);
    return d->sendQueryRequest(query, callback);
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
