#pragma once

#include <functional>

#include <QtCore/QElapsedTimer>

#include <client_core/connection_context_aware.h>

#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <utils/common/id.h>
#include <utils/common/connective.h>

class QnCameraBookmarksManagerPrivate:
    public Connective<QObject>,
    public QnConnectionContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnCameraBookmarksManagerPrivate(QnCameraBookmarksManager *parent);

    virtual ~QnCameraBookmarksManagerPrivate();

    bool isEnabled() const;
    void setEnabled(bool value);

    /* Direct API section */

    /// @brief                  Asynchronously gathers bookmarks using specified filter.
    /// @param cameras          Set of target cameras.
    /// @param filter           Filter parameters.
    /// @param callback         Callback for receiving bookmarks data.
    /// @returns                Internal id of the request.
    int getBookmarksAsync(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter, BookmarksInternalCallbackType callback);

    /// @brief                  Add the bookmark to the camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void addCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Update the existing bookmark on the camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void updateCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Delete the existing bookmark from the camera.
    /// @param bookmarkId       Target bookmark id.
    /// @param callback         Callback with operation result.
    void deleteCameraBookmark(const QnUuid &bookmarkId, OperationCallbackType callback = OperationCallbackType());

    /* Queries API section */

    /// @brief                  Instantiate new search query.
    /// @param cameras          Set of target cameras.
    /// @param filter           Filter parameters.
    /// @returns                Search query that is ready to be used.
    QnCameraBookmarksQueryPtr createQuery(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter);

    /// @brief                  Get search query cached value from local data.
    /// @param query            Target query.
    /// @returns                Locally cached bookmarks, filtered by the query.
    QnCameraBookmarkList cachedBookmarks(const QnCameraBookmarksQueryPtr &query) const;

    /// @brief                  Asynchronously execute search query on remote (actual) data.
    /// @param query            Target query.
    /// @param callback         Callback for receiving bookmarks data.
    void executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback);
private slots:

    void handleDataLoaded(int status, const QnCameraBookmarkList &bookmarks, int requestId);

    void handleBookmarkOperation(int status, int handle);
private:
    /// @brief                  Register bookmarks search query to auto-update it if needed.
    /// @param query            Target query.
    void registerQuery(const QnCameraBookmarksQueryPtr &query);

    /// @brief                  Unregister bookmarks search query.
    /// @param queryId          Target query id.
    void unregisterQuery(const QUuid &queryId);

    /// @brief                  Clear all local cache data.
    void clearCache();

    /// @brief                  Check if the single query should be updated.
    /// @param queryId          Target query id.
    /// @returns                True if the query is queued to update.
    bool isQueryUpdateRequired(const QUuid &queryId);

    /// @brief                  Check if queries should be updated and update if needed.
    void checkQueriesUpdate();

    /// @brief                  Send request to update query with actual data.
    void updateQueryAsync(const QUuid &queryId);

    /// @brief                  Update cache with provided value and emit signals if needed.
    void updateQueryCache(const QUuid &queryId, const QnCameraBookmarkList &bookmarks);

    void executeCallbackDelayed(BookmarksInternalCallbackType callback);
    void executeCallbackDelayed(BookmarksCallbackType callback);

    /** @brief                  Add the added or updated bookmark to the pending bookmarks list.
     *  Pending bookmark is the bookmark that was created, modified or deleted on the client
     *  but these changes haven't been reflected to queries caches.
     *  Pending bookmarks live inside the bookmarks manager for some time to let the queries gather actual information.
     */
    void addUpdatePendingBookmark(const QnCameraBookmark &bookmark);
    /// @brief                  Add removed bookmark to the pending bookmarks list.
    void addRemovePendingBookmark(const QnUuid &bookmarkId);
    /// @brief                  Merge the pending bookmarks with the given bookmarks list.
    /// @param query            The query for with merge operation is performed. The query specifies filters and cameras list.
    void mergeWithPendingBookmarks(const QnCameraBookmarksQueryPtr query, QnCameraBookmarkList &bookmarks);

    /// @brief                  Find and discard timed-out pending bookmarks.
    void checkPendingBookmarks();

    /// @brief                  Find and discard camera from all cached queries. Invalid queries will be discarded as well.
    void removeCameraFromQueries(const QnResourcePtr& resource);

private:
    Q_DECLARE_PUBLIC(QnCameraBookmarksManager)
    QnCameraBookmarksManager *q_ptr;

    QTimer* m_operationsTimer;

    QMap<int, BookmarksInternalCallbackType> m_requests;

    struct OperationInfo {
        enum class OperationType {
            Add,
            Update,
            Delete
        };

        OperationType operation;
        OperationCallbackType callback;
        QnUuid bookmarkId;

        OperationInfo();
        OperationInfo(OperationType operation, const QnUuid &bookmarkId, OperationCallbackType callback);
    };

    QMap<int, OperationInfo> m_operations;

    struct QueryInfo {
        enum class QueryState {
            Invalid,                                        /**< Query was not requested yet. */
            Queued,                                         /**< Query will be requested when timeout will pass. */
            Requested,                                      /**< Waiting for the server response. */
            Actual                                          /**< Data is actual. */
        };

        QnCameraBookmarksQueryWeakPtr   queryRef;           /**< Weak reference to Query object. */
        QnCameraBookmarkList            bookmarksCache;     /**< Cached bookmarks. */
        QueryState                      state;              /**< Current query state. */
        QElapsedTimer                   requestTimer;       /**< Time of the last request. */
        int                             requestId;          /**< Id of the last request. */

        QueryInfo();
        QueryInfo(const QnCameraBookmarksQueryPtr &query);
    };

    /** Cached bookmarks by query. */
    QHash<QUuid, QueryInfo> m_queries;

    struct PendingInfo {
        QnCameraBookmark bookmark;
        QElapsedTimer discardTimer;

        enum Type {
            AddBookmark,
            RemoveBookmark
        };

        Type type;

        PendingInfo(const QnCameraBookmark &bookmark, Type type);
        PendingInfo(const QnUuid &bookmarkId, Type type);
    };

    /* Cache for case when we have sent getBookmarks request and THEN added/removed a bookmark
     * locally. So when we've got reply for getBookmarks, we may fix it correspondingly. */
    QHash<QnUuid, PendingInfo> m_pendingBookmarks;
};
