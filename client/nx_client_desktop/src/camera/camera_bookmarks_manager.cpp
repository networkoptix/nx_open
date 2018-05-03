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

bool QnCameraBookmarksManager::isEnabled() const
{
    Q_D(const QnCameraBookmarksManager);
    return d->isEnabled();
}

void QnCameraBookmarksManager::setEnabled(bool value)
{
    Q_D(QnCameraBookmarksManager);
    d->setEnabled(value);
}

void QnCameraBookmarksManager::getBookmarksAsync(const QnVirtualCameraResourceSet &cameras
    , const QnCameraBookmarkSearchFilter &filter
    , BookmarksCallbackType callback)
{
    Q_D(QnCameraBookmarksManager);
    auto internalCallback = [callback](bool success, const QnCameraBookmarkList &bookmarks, int requestId)
    {
        Q_UNUSED(requestId);
        if (callback)
            callback(success, bookmarks);
    };
    d->getBookmarksAsync(cameras, filter, internalCallback);
}

int QnCameraBookmarksManager::getBookmarksAsync(const QnVirtualCameraResourceSet &cameras
    , const QnCameraBookmarkSearchFilter &filter
    , BookmarksInternalCallbackType callback)
{
    Q_D(QnCameraBookmarksManager);
    return d->getBookmarksAsync(cameras, filter, callback);
}

void QnCameraBookmarksManager::addCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), Q_FUNC_INFO, "Invalid bookmark");
    Q_D(QnCameraBookmarksManager);
    d->addCameraBookmark(bookmark, callback);

    emit bookmarkAdded(bookmark);
}

void QnCameraBookmarksManager::addAcknowledge(
    const QnCameraBookmark &bookmark,
    const QnUuid& eventRuleId,
    OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), Q_FUNC_INFO, "Invalid bookmark");
    Q_D(QnCameraBookmarksManager);
    d->acknowledgeEvent(bookmark, eventRuleId, callback);

    emit bookmarkAdded(bookmark);
}

void QnCameraBookmarksManager::updateCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), Q_FUNC_INFO, "Invalid bookmark");
    Q_D(QnCameraBookmarksManager);
    d->updateCameraBookmark(bookmark, callback);

    emit bookmarkUpdated(bookmark);
}

void QnCameraBookmarksManager::deleteCameraBookmark(const QnUuid &bookmarkId, OperationCallbackType callback)
{
    Q_D(QnCameraBookmarksManager);
    d->deleteCameraBookmark(bookmarkId, callback);

    emit bookmarkRemoved(bookmarkId);
}

QnCameraBookmarkList QnCameraBookmarksManager::cachedBookmarks(const QnCameraBookmarksQueryPtr &query) const
{
    NX_ASSERT(query);
    Q_D(const QnCameraBookmarksManager);
    return d->cachedBookmarks(query);
}

void QnCameraBookmarksManager::executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback)
{
    NX_ASSERT(query);
    Q_D(QnCameraBookmarksManager);
    d->executeQueryRemoteAsync(query, callback);
}

QnCameraBookmarksQueryPtr QnCameraBookmarksManager::createQuery(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter)
{
    Q_D(QnCameraBookmarksManager);
    return d->createQuery(cameras, filter);
}
