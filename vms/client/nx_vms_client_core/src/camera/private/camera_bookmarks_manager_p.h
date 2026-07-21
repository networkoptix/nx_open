// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include <api/rest_types.h>
#include <api/server_rest_connection_fwd.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/vms/client/core/system_context_aware.h>
#include <nx/vms/event/event_fwd.h>

struct QnMultiserverRequestData;

namespace nx::vms::api { struct BookmarkV3; }

class QnCameraBookmarksManagerPrivate:
    public QObject,
    public nx::vms::client::core::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnCameraBookmarksManagerPrivate(
        nx::vms::client::core::SystemContext* systemContext,
        QObject* parent = nullptr);

    virtual ~QnCameraBookmarksManagerPrivate() override;

    using RawResponseType =
        std::function<void (bool, rest::Handle, QByteArray, const nx::network::http::HttpHeaders&)>;

    // Direct API section.

    /**
     * Asynchronously gathers bookmarks using specified filter.
     * @param filter Filter parameters.
     * @param callback Callback for receiving bookmarks data.
     * @param executor Callback execution thread control object.
     * @return Internal id of the request.
     */
    int getBookmarksAsync(const QnCameraBookmarkSearchFilter& filter,
        BookmarksCallbackType callback, nx::utils::AsyncHandlerExecutor executor);

    /**
     * Gathers bookmarks around specified time point using compatibility bookmark request.
     */
    int getBookmarksAroundPoint_compatibilityMode(
        const QnCameraBookmarkSearchFilter& filter,
        BookmarksCallbackType callback);

    int getBookmarkTagsAsync(int maxTags, BookmarkTagsCallbackType callback);

    // Queries API section.

    /** Adds or updates the bookmark in queries caches. */
    void addOrUpdateBookmarkInQueries(const QnCameraBookmark& bookmark);

    /** Removes the bookmark from queries caches. */
    void removeCameraBookmarkFromQueries(const nx::Uuid& bookmarkId);

    /**
     * Instantiate new search query.
     * @param filter Filter parameters.
     * @return Search query that is ready to be used.
     */
    QnCameraBookmarksQueryPtr createQuery(const QnCameraBookmarkSearchFilter& filter);

    /**
     * Asynchronously execute search query.
     * @param query Target query.
     * @param callback Callback for received bookmarks data. Will be called when data is fully
     *     loaded.
     * @returns Request ID.
     */
    rest::Handle sendQueryRequest(
        const QnCameraBookmarksQueryPtr& query,
        BookmarksCallbackType callback);

    /**
     * Asynchronously execute search query, requesting data only from the given timepoint.
     * @param query Target query.
     * @param timePoint Minimal start time for the request filter
     * @param callback Callback for received bookmarks data. Will be called when data is fully
     *     loaded.
     */
    void sendQueryTailRequest(
        const QnCameraBookmarksQueryPtr& query,
        std::chrono::milliseconds timePoint,
        BookmarksCallbackType callback);

    using RestRequestCallback = rest::Callback<rest::ErrorOrData<QByteArray>>;
    rest::Handle sendRestRequest(
        const QString& path,
        const nx::vms::api::BookmarkV3& bookmark,
        const nx::network::http::Method& method,
        RestRequestCallback&& callback);

private:
    void handleQueryReply(
        const QUuid& queryId,
        bool success,
        int requestId,
        QnCameraBookmarkList bookmarks,
        BookmarksCallbackType callback);

    /**
     * Register bookmarks search query to auto-update it if needed.
     * @param query Target query.
     */
    void registerQuery(const QnCameraBookmarksQueryPtr& query);

    /**
     * Unregister bookmarks search query.
     * @param queryId Target query id.
     */
    void unregisterQuery(const QUuid& queryId);

    /**
     * Check if the single query should be updated.
     * @param queryId Target query id.
     * @return True if the query is queued to update.
     */
    bool isQueryUpdateRequired(const QUuid& queryId);

    /** Check if queries should be updated and update if needed. */
    void checkQueriesUpdate();

    /** Send request to update query with actual data. */
    void updateQueryAsync(const QUuid& queryId);

    /**
     * Find and discard camera from all cached queries. Invalid queries will be discarded
     * as well.
     */
    void removeCameraFromQueries(const QnResourcePtr& resource);

    int sendGetRequest(const QString& path, QnMultiserverRequestData& request,
        RawResponseType callback);

    void startOperationsTimer();

private:
    QTimer* m_operationsTimer;

    struct QueryInfo
    {
        /** Weak reference to Query object. */
        QnCameraBookmarksQueryWeakPtr queryRef;

        /** Time of the last request. */
        QElapsedTimer requestTimer;

        /** Id of the last request. */
        rest::Handle requestId;

        QueryInfo();
        QueryInfo(const QnCameraBookmarksQueryPtr& query);
    };

    /** Cached bookmarks by query. */
    QHash<QUuid, QueryInfo> m_queries;
};
