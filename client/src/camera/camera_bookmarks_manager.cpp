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

void QnCameraBookmarksManager::getBookmarksAsync(const QnVirtualCameraResourceList &cameras
                                                 , const QnCameraBookmarkSearchFilter &filter
                                                 , BookmarksCallbackType callback)
{
    Q_D(QnCameraBookmarksManager);
    d->getBookmarksAsync(cameras, filter, callback);
}

void QnCameraBookmarksManager::addCameraBookmark(const QnVirtualCameraResourcePtr &camera, const QnCameraBookmark &bookmark, OperationCallbackType callback) {
    Q_D(QnCameraBookmarksManager);
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

void QnCameraBookmarksManager::registerAutoUpdateQuery(const QnCameraBookmarksQueryPtr &query) {
    Q_D(QnCameraBookmarksManager);
    d->registerAutoUpdateQuery(query);
}

void QnCameraBookmarksManager::unregisterAutoUpdateQuery(const QnCameraBookmarksQueryPtr &query) {
    Q_D(QnCameraBookmarksManager);
    d->unregisterAutoUpdateQuery(query);
}

QnCameraBookmarkList QnCameraBookmarksManager::executeQueryLocal(const QnCameraBookmarksQueryPtr &query) const {
    Q_D(const QnCameraBookmarksManager);
    return d->executeQuery(query);
}

void QnCameraBookmarksManager::executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback) {
    getBookmarksAsync(query->cameras(), query->filter(), callback);
}
