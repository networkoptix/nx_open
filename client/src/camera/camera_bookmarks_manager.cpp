#include "camera_bookmarks_manager.h"

#include <camera/camera_bookmarks_query.h>

#include <camera/private/camera_bookmarks_manager_p.h>

QnCameraBookmarksManager::QnCameraBookmarksManager(QObject *parent)
    : QObject(parent)
    , d_ptr(new QnCameraBookmarksManagerPrivate(this))
{
}

QnCameraBookmarksManager::~QnCameraBookmarksManager()
{
}

void QnCameraBookmarksManager::getBookmarksAsync(const QnVirtualCameraResourceSet &cameras
                                                 , const QnCameraBookmarkSearchFilter &filter
                                                 , BookmarksCallbackType callback)
{
    Q_D(QnCameraBookmarksManager);
    auto internalCallback = [callback](bool success, const QnCameraBookmarkList &bookmarks, int requestId) {
        Q_UNUSED(requestId);
        if (callback)
            callback(success, bookmarks);
    };
    d->getBookmarksAsync(cameras, filter, internalCallback);
}


QnCameraBookmarkList QnCameraBookmarksManager::getLocalBookmarks(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter) const {
    Q_D(const QnCameraBookmarksManager);
    return d->getLocalBookmarks(cameras, filter);
}


void QnCameraBookmarksManager::addCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback) {
    Q_D(QnCameraBookmarksManager);

    Q_ASSERT_X(!bookmark.cameraId.isEmpty(), Q_FUNC_INFO, "Camera ID should not be empty!");

    d->addCameraBookmark(camera, bookmark, callback);
}

void QnCameraBookmarksManager::updateCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback) {
    Q_D(QnCameraBookmarksManager);
    d->updateCameraBookmark(camera, bookmark, callback);
}

void QnCameraBookmarksManager::deleteCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback) {
    Q_D(QnCameraBookmarksManager);
    d->deleteCameraBookmark(camera, bookmark, callback);
}

QnCameraBookmarkList QnCameraBookmarksManager::executeQueryLocal(const QnCameraBookmarksQueryPtr &query) const {
    Q_D(const QnCameraBookmarksManager);
    return d->executeQueryLocal(query);
}

void QnCameraBookmarksManager::executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback) {
    Q_D(QnCameraBookmarksManager);
    d->executeQueryRemoteAsync(query, callback);
}

QnCameraBookmarksQueryPtr QnCameraBookmarksManager::createQuery(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter) {
    Q_D(QnCameraBookmarksManager);
    return d->createQuery(cameras, filter);
}
