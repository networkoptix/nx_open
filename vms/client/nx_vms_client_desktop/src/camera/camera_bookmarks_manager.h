#pragma once

#include <functional>

#include <core/resource/camera_bookmark.h>

#include <camera/camera_bookmarks_manager_fwd.h>

#include <nx/utils/singleton.h>

#include <nx/vms/event/event_fwd.h>

/** Singleton, used to create, update, delete, fetch and cache bookmarks. */
class QnCameraBookmarksManager : public QObject, public Singleton<QnCameraBookmarksManager>
{
    Q_OBJECT
public:
    QnCameraBookmarksManager(QObject *parent = nullptr);

    virtual ~QnCameraBookmarksManager();

    bool isEnabled() const;
    void setEnabled(bool value);

     /* Direct API section */

    /// @brief                  Asynchronously gathers bookmarks using specified filter.
    /// @param cameras          Set of target cameras.
    /// @param filter           Filter parameters.
    /// @param callback         Callback for receiving bookmarks data.
    void getBookmarksAsync(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter, BookmarksCallbackType callback);

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

    /// @brief                  Add the bookmark to the camera and stores record in event log
    ///                         for the specified event.
    /// @param bookmark         Target bookmark.
    /// @param businessRuleId   Business rule identifier for which bookmark is created
    /// @param callback         Callback with operation result.
    void addAcknowledge(
        const QnCameraBookmark &bookmark,
        const QnUuid& eventRuleId,
        OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Update the existing bookmark on the camera.
    /// @param bookmark         Target bookmark.
    /// @param callback         Callback with operation result.
    void updateCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback = OperationCallbackType());

    /// @brief                  Delete the existing bookmark from the camera.
    /// @param bookmark         Target bookmark id.
    /// @param callback         Callback with operation result.
    void deleteCameraBookmark(const QnUuid &bookmarkId, OperationCallbackType callback = OperationCallbackType());

    /* Queries API section */

    /// @brief                  Instantiate new search query.
    /// @param cameras          Set of target cameras.
    /// @param filter           Filter parameters.
    /// @returns                Search query that is ready to be used.
    QnCameraBookmarksQueryPtr createQuery(const QnVirtualCameraResourceSet &cameras = QnVirtualCameraResourceSet(),
                                          const QnCameraBookmarkSearchFilter &filter = QnCameraBookmarkSearchFilter());

    /// @brief                  Get locally cached bookmarks for the given query.
    /// @param query            Target query.
    QnCameraBookmarkList cachedBookmarks(const QnCameraBookmarksQueryPtr &query) const;

    /// @brief                  Asynchronously execute search query on remote (actual) data.
    /// @param query            Target query.
    /// @param callback         Callback for receiving bookmarks data.
    void executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback);

signals:
    /* TODO: #dklychkov #2.6 #bookmarks Implement notification transactions bor bookmarks which will call these signals. */
    // So far these signals are emited when the user does something locally.
    /// @brief                  This signal is emitted when new bookmark was added.
    void bookmarkAdded(const QnCameraBookmark &bookmark);
    /// @brief                  This signal is emitted when the bookmark was updated.
    void bookmarkUpdated(const QnCameraBookmark &bookmark);
    /// @brief                  This signal is emitted when the bookmark was removed.
    /// @param                  The removed bookmark GUID.
    void bookmarkRemoved(const QnUuid &bookmarkId);

protected:
    Q_DECLARE_PRIVATE(QnCameraBookmarksManager);

private:
    QScopedPointer<QnCameraBookmarksManagerPrivate> d_ptr;
};

#define qnCameraBookmarksManager QnCameraBookmarksManager::instance()
