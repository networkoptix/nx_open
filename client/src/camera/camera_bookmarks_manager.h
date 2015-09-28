#pragma once

#include <functional>

#include <core/resource/camera_bookmark.h>

#include <camera/camera_bookmarks_manager_fwd.h>

#include <utils/common/singleton.h>

/** Singleton, used to create, update, delete, fetch and cache bookmarks. */
class QnCameraBookmarksManager : public QObject, public Singleton<QnCameraBookmarksManager>
{
    Q_OBJECT
public:
    QnCameraBookmarksManager(QObject *parent = nullptr);

    virtual ~QnCameraBookmarksManager();

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
    QnCameraBookmarksQueryPtr createQuery(const QnVirtualCameraResourceSet &cameras = QnVirtualCameraResourceSet(),
                                          const QnCameraBookmarkSearchFilter &filter = QnCameraBookmarkSearchFilter());

    /// @brief                  Execute search query on local (cached) data.
    /// @param query            Target query.
    /// @returns                Locally cached bookmarks, filtered by the query.
    QnCameraBookmarkList executeQueryLocal(const QnCameraBookmarksQueryPtr &query) const;

    /// @brief                  Asynchronously execute search query on remote (actual) data.
    /// @param query            Target query.
    /// @param callback         Callback for receiving bookmarks data.
    void executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback);
protected:
    Q_DECLARE_PRIVATE(QnCameraBookmarksManager);

private:
    QScopedPointer<QnCameraBookmarksManagerPrivate> d_ptr;
};

#define qnCameraBookmarksManager QnCameraBookmarksManager::instance()
