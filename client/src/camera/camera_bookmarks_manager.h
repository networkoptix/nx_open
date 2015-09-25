#pragma once

#include <functional>

#include <camera/camera_bookmarks_manager_fwd.h>

#include <utils/common/singleton.h>

class QnCameraBookmarksManager : public QObject, public Singleton<QnCameraBookmarksManager>
{
    Q_OBJECT
public:
    QnCameraBookmarksManager(QObject *parent = nullptr);

    virtual ~QnCameraBookmarksManager();

     /* Direct API section */

    /// @brief Asynchronously gathers bookmarks using specified filter
    /// @param cameras          List of target cameras.
    /// @param filter           Filter parameters.
    /// @param callback         Callback for receiving bookmarks data.
    void getBookmarksAsync(const QnVirtualCameraResourceList &cameras, const QnCameraBookmarkSearchFilter &filter, BookmarksCallbackType callback);

    /// @brief Add the bookmark to the camera.
    /// @param camera           Target camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void addCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /// @brief Update the existing bookmark on the camera.
    /// @param camera           Target camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void updateCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /// @brief Delete the existing bookmark from the camera.
    /// @param camera           Target camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void deleteCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /* Queries API section */

    /// @brief Register bookmarks search query to auto-update it.
    /// @param query            Target query.
    void registerAutoUpdateQuery(const QnCameraBookmarksQueryPtr &query);

    /// @brief Unregister bookmarks search query.
    /// @param query            Target query.
    void unregisterAutoUpdateQuery(const QnCameraBookmarksQueryPtr &query);

    /// @brief Execute search query on local (cached) data.
    /// @param query            Target query.
    QnCameraBookmarkList executeQueryLocal(const QnCameraBookmarksQueryPtr &query) const;

    /// @brief Asynchronously execute search query on remote (actual) data.
    /// @param query            Target query.
    void executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback);
protected:
    Q_DECLARE_PRIVATE(QnCameraBookmarksManager);

private:
    QScopedPointer<QnCameraBookmarksManagerPrivate> d_ptr;
};

#define qnCameraBookmarksManager QnCameraBookmarksManager::instance()
