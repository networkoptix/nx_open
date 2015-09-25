#pragma once

#include <functional>

#include <camera/camera_bookmarks_manager_fwd.h>

class QnCameraBookmarksManagerPrivate : public QObject {
    Q_OBJECT
public:
    QnCameraBookmarksManagerPrivate(QnCameraBookmarksManager *parent);
    
    virtual ~QnCameraBookmarksManagerPrivate();

    void getBookmarksAsync(const QnVirtualCameraResourceList &cameras, const QnCameraBookmarkSearchFilter &filter, BookmarksCallbackType callback);
    void addCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback);
    void updateCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback);
    void deleteCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback);

    /// @brief Register bookmarks search query to auto-update it.
    /// @param query            Target query.
    void registerAutoUpdateQuery(const QnCameraBookmarksQueryPtr &query);

    /// @brief Unregister bookmarks search query.
    /// @param query            Target query.
    void unregisterAutoUpdateQuery(const QnCameraBookmarksQueryPtr &query);

    /// @brief Execute search query on local data.
    /// @param query            Target query.
    QnCameraBookmarkList executeQuery(const QnCameraBookmarksQueryPtr &query) const;
private slots:
    void handleDataLoaded(int status, const QnCameraBookmarkList &bookmarks, int handle);

private:
    void clearCache();
    void updateQueryCache(const QnCameraBookmarksQueryPtr &query);
    QnCameraBookmarkList executeQueryInternal(const QnCameraBookmarksQueryPtr &query) const;

private:
    Q_DECLARE_PUBLIC(QnCameraBookmarksManager)
    QnCameraBookmarksManager *q_ptr;

    QMap<int, BookmarksCallbackType> m_requests;

    /** Cached bookmarks by query. */
    QHash<QnCameraBookmarksQueryPtr, QnCameraBookmarkList> m_autoUpdatingQueries;

    QHash<QnVirtualCameraResourcePtr, QnCameraBookmarkList> m_bookmarksByCamera;
};