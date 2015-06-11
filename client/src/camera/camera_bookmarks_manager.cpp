#include "camera_bookmarks_manager.h"

#include <camera/private/camera_bookmarks_manager_p.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <camera/loaders/bookmark_camera_data_loader.h>

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
                                                 , const QUuid &requestId
                                                 , BookmarksCallbackType callback)
{
    Q_D(QnCameraBookmarksManager);
    d->getBookmarksAsync(cameras, filter, requestId, callback);
}

QnBookmarksLoader * QnCameraBookmarksManager::loader(const QnVirtualCameraResourcePtr &camera, bool createIfNotExists /*= true*/) {
    Q_D(QnCameraBookmarksManager);
    return d->loader(camera, createIfNotExists);
}

void QnCameraBookmarksManager::clearCache() {
    Q_D(QnCameraBookmarksManager);
    d->clearCache();
}

QnCameraBookmarkList QnCameraBookmarksManager::bookmarks(const QnVirtualCameraResourcePtr &camera) const {
    const Q_D(QnCameraBookmarksManager);
    return d->bookmarks(camera);
}
