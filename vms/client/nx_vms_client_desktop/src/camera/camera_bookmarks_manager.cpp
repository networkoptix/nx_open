// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmarks_manager.h"

#include <camera/camera_bookmarks_query.h>

#include <camera/private/camera_bookmarks_manager_p.h>

template<> QnCameraBookmarksManager* Singleton<QnCameraBookmarksManager>::s_instance = nullptr;

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

int QnCameraBookmarksManager::getBookmarksAsync(const QnCameraBookmarkSearchFilter& filter,
    BookmarksCallbackType callback)
{
    Q_D(QnCameraBookmarksManager);
    return d->getBookmarksAsync(filter, callback);
}

int QnCameraBookmarksManager::getBookmarkTagsAsync(int maxTags, BookmarkTagsCallbackType callback)
{
    Q_D(QnCameraBookmarksManager);
    return d->getBookmarkTagsAsync(maxTags, callback);
}

void QnCameraBookmarksManager::addCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark");
    Q_D(QnCameraBookmarksManager);
    d->addCameraBookmark(bookmark, callback);

    emit bookmarkAdded(bookmark);
}

void QnCameraBookmarksManager::addAcknowledge(
    const QnCameraBookmark &bookmark,
    const QnUuid& eventRuleId,
    OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark");
    Q_D(QnCameraBookmarksManager);
    d->acknowledgeEvent(bookmark, eventRuleId, callback);

    emit bookmarkAdded(bookmark);
}

void QnCameraBookmarksManager::updateCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark");
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

QnCameraBookmarksQueryPtr QnCameraBookmarksManager::createQuery(const QnCameraBookmarkSearchFilter& filter)
{
    Q_D(QnCameraBookmarksManager);
    return d->createQuery(filter);
}
