// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmarks_manager.h"

#include <camera/camera_bookmarks_query.h>
#include <camera/private/camera_bookmarks_manager_p.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/client/core/bookmarks/bookmark_utils.h>
#include <nx/vms/client/core/event_search/utils/event_search_item_helper.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/log_strings_format.h>
#include <nx/vms/common/api/helpers/bookmark_api_converter.h>
#include <nx/vms/common/bookmark/bookmark_facade.h>

QnCameraBookmarksManager::QnCameraBookmarksManager(
    nx::vms::client::core::SystemContext* systemContext,
    QObject* parent)
    :
    base_type(parent),
    d(new QnCameraBookmarksManagerPrivate(systemContext))
{
}

QnCameraBookmarksManager::~QnCameraBookmarksManager()
{
}

int QnCameraBookmarksManager::getBookmarksAsync(const QnCameraBookmarkSearchFilter& filter,
    BookmarksCallbackType callback, nx::utils::AsyncHandlerExecutor executor)
{
    return d->getBookmarksAsync(filter, callback, executor);
}

int QnCameraBookmarksManager::getBookmarksAroundPointAsync(
    const QnCameraBookmarkSearchFilter& filter,
    const QnCameraBookmarkList& source,
    BookmarksAroundPointCallbackType&& bookmarkCallback)
{
    if (!NX_ASSERT(filter.centralTimePointMs))
        return 0;

    NX_ASSERT(filter.orderBy.column == nx::vms::api::BookmarkSortField::startTime);

    static const auto kCentralPointSupportApiVersion = nx::utils::SoftwareVersion(6, 1, 4);
    const bool centralPointSupported =
        d->systemContext()->moduleInformation().version >= kCentralPointSupportApiVersion;

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

    return centralPointSupported
        ? getBookmarksAsync(filter, std::move(bookmarksCallback), d.get())
        : d->getBookmarksAroundPoint_compatibilityMode(filter, std::move(bookmarksCallback));
}

int QnCameraBookmarksManager::getBookmarkTagsAsync(int maxTags, BookmarkTagsCallbackType callback)
{
    return d->getBookmarkTagsAsync(maxTags, callback);
}

void QnCameraBookmarksManager::addExistingBookmark(const QnCameraBookmark& bookmark)
{
    if (!NX_ASSERT(bookmark.isValid(), "Invalid bookmark"))
        return;

    d->addOrUpdateBookmarkInQueries(bookmark);
    emit bookmarkAdded(bookmark);
}

bool QnCameraBookmarksManager::submitBookmarkOperation(BookmarkOperation operation,
    const QnCameraBookmark& bookmark,
    OperationCallbackType&& callback)
{
    if (!NX_ASSERT(bookmark.isValid(), "Invalid bookmark cannot be added, updated or removed"))
        return false;

    const auto server =
        nx::vms::client::core::bookmarks::getServerForBookmark(bookmark, d->systemContext());
    if (!server || server->getStatus() != nx::vms::api::ResourceStatus::online)
        return false;

    const auto apiBookmark = nx::vms::common::bookmarkToApi<nx::vms::api::BookmarkV3>(
        bookmark, server->getId(), /*includeDigest*/ true);

    QString path;
    nx::network::http::Method method;
    QString errorMessage;

    switch (operation)
    {
        case BookmarkOperation::create:
            path = nx::format("rest/v4/devices/%1/bookmarks", apiBookmark.deviceId);
            method = nx::network::http::Method::post;
            errorMessage = "Cannot create bookmark";
            break;

        case BookmarkOperation::update:
            path = nx::format("rest/v4/devices/%1/bookmarks/%2", apiBookmark.deviceId,
                apiBookmark.id);
            method = nx::network::http::Method::patch;
            errorMessage = "Cannot update bookmark";
            break;

        case BookmarkOperation::remove:
            path = nx::format("rest/v4/devices/*/bookmarks/%1", apiBookmark.id);
            method = nx::network::http::Method::delete_;
            errorMessage = "Cannot delete bookmark";
            break;
    }

    auto restCallback =
        [this, operation, errorMessage, operationCallback = std::move(callback),
            bookmarkId = bookmark.guid]
        (bool success, rest::Handle /*handle*/, rest::ErrorOrData<QByteArray> response)
        {
            NX_LOG_RESPONSE(this, success, response, errorMessage);

            if (!success)
            {
                if (operationCallback)
                    operationCallback(false, {});
                return;
            }

            std::optional<nx::vms::api::BookmarkV3> operationResult;
            if (operation == BookmarkOperation::create || operation == BookmarkOperation::update)
            {
                operationResult = nx::vms::api::BookmarkV3();
                if (!response.has_value() || !QJson::deserialize<nx::vms::api::BookmarkV3>(
                    response.value(), &*operationResult))
                {
                    NX_ASSERT(false, "Unexpected response");
                    if (operationCallback)
                        operationCallback(false, {});
                    return;
                }
            }

            if (operationCallback)
                operationCallback(true, operationResult);

            switch (operation)
            {
                case BookmarkOperation::create:
                {
                    const auto bookmark = nx::vms::common::bookmarkFromApi(operationResult.value());
                    d->addOrUpdateBookmarkInQueries(bookmark);
                    emit bookmarkAdded(bookmark);
                    break;
                }

                case BookmarkOperation::update:
                {
                    const auto bookmark = nx::vms::common::bookmarkFromApi(operationResult.value());
                    d->addOrUpdateBookmarkInQueries(bookmark);
                    emit bookmarkUpdated(bookmark);
                    break;
                }

                case BookmarkOperation::remove:
                    d->removeCameraBookmarkFromQueries(bookmarkId);
                    emit bookmarkRemoved(bookmarkId);
                    break;
            }
        };

    return d->sendRestRequest(path, apiBookmark, method, std::move(restCallback)) != rest::Handle();
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

QnCameraBookmarksQueryPtr QnCameraBookmarksManager::createQuery(
    const QnCameraBookmarkSearchFilter& filter)
{
    return d->createQuery(filter);
}
