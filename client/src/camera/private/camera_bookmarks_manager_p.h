#pragma once

#include <functional>

#include <camera/camera_bookmarks_manager_fwd.h>

#include <utils/common/connective.h>

class QnCameraBookmarksManagerPrivate : public Connective<QObject> {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnCameraBookmarksManagerPrivate(QnCameraBookmarksManager *parent);
    
    virtual ~QnCameraBookmarksManagerPrivate();

    /* Direct API section */

    /// @brief                  Asynchronously gathers bookmarks using specified filter.
    /// @param cameras          Set of target cameras.
    /// @param filter           Filter parameters.
    /// @param callback         Callback for receiving bookmarks data.
    void getBookmarksAsync(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter, BookmarksCallbackType callback);

    /// @brief                  Synchronously gathers locally cached bookmarks using specified filter.
    /// @param cameras          Set of target cameras.
    /// @param filter           Filter parameters.
    /// @returns                Locally cached bookmarks, filtered by the cameras and filter.
    QnCameraBookmarkList getLocalBookmarks(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter) const;

    /// @brief                  Add the bookmark to the camera.
    /// @param camera           Target camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void addCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Update the existing bookmark on the camera.
    /// @param camera           Target camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void updateCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Delete the existing bookmark from the camera.
    /// @param camera           Target camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void deleteCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /* Queries API section */

    /// @brief                  Instantiate new search query.
    /// @param cameras          Set of target cameras.
    /// @param filter           Filter parameters.
    /// @returns                Search query that is ready to be used.
    QnCameraBookmarksQueryPtr createQuery(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter);

    /// @brief                  Get search query cached value from local data.
    /// @param query            Target query.
    /// @returns                Locally cached bookmarks, filtered by the query.
    QnCameraBookmarkList executeQueryLocal(const QnCameraBookmarksQueryPtr &query) const;

    /// @brief                  Asynchronously execute search query on remote (actual) data.
    /// @param query            Target query.
    /// @param callback         Callback for receiving bookmarks data.
    void executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback);
private slots:

    void handleDataLoaded(int status, const QnCameraBookmarkList &bookmarks, int handle);

    void handleBookmarkOperation(int status, const QnCameraBookmark &bookmark, int handle);
private:
    /// @brief                  Register bookmarks search query to auto-update it if needed.
    /// @param query            Target query.
    void registerQuery(const QnCameraBookmarksQueryPtr &query);

    /// @brief                  Unregister bookmarks search query.
    /// @param queryId          Target query id.
    void unregisterQuery(const QUuid &queryId);

    void clearCache();

    /// @brief                  Update query cached data based on local cache.
    void updateQueryLocal(const QUuid &queryId);

    /// @brief                  Send request to update query with actual data.
    void updateQueryAsync(const QUuid &queryId);

    /// @brief                  Update cache with provided value and emit signals if needed.
    void updateQueryCache(const QUuid &queryId, const QnCameraBookmarkList &bookmarks);

    /// @brief                  Force recalculate value based on local data.
    QnCameraBookmarkList executeQueryInternal(const QnCameraBookmarksQueryPtr &query) const;

    void executeCallbackDelayed(BookmarksCallbackType callback);
private:
    Q_DECLARE_PUBLIC(QnCameraBookmarksManager)
    QnCameraBookmarksManager *q_ptr;

    QMap<int, BookmarksCallbackType> m_requests;

    struct OperationInfo {
        enum class OperationType {
            Add,
            Update,
            Delete
        };

        OperationType operation;
        OperationCallbackType callback;
        QnVirtualCameraResourcePtr camera;

        OperationInfo();
        OperationInfo(OperationType operation, OperationCallbackType callback, const QnVirtualCameraResourcePtr &camera);
    };

    QMap<int, OperationInfo> m_operations;

    struct QueryInfo {
        QnCameraBookmarksQueryWeakPtr queryRef;
        QnCameraBookmarkList bookmarksCache;        

        QueryInfo();
        QueryInfo(const QnCameraBookmarksQueryPtr &query);
    };

    /** Cached bookmarks by query. */
    QHash<QUuid, QueryInfo> m_queries;

    QHash<QnVirtualCameraResourcePtr, QnCameraBookmarkList> m_bookmarksByCamera;
};