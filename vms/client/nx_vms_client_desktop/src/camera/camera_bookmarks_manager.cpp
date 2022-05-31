// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmarks_manager.h"

#include <camera/camera_bookmarks_query.h>
#include <camera/private/camera_bookmarks_manager_p.h>

using namespace nx::vms::client::desktop;

QnCameraBookmarksManager::QnCameraBookmarksManager(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    d(new QnCameraBookmarksManagerPrivate(systemContext))
{
}

QnCameraBookmarksManager::~QnCameraBookmarksManager()
{
}

bool QnCameraBookmarksManager::isEnabled() const
{
    return d->isEnabled();
}

void QnCameraBookmarksManager::setEnabled(bool value)
{
    d->setEnabled(value);
}

int QnCameraBookmarksManager::getBookmarksAsync(const QnCameraBookmarkSearchFilter& filter,
    BookmarksCallbackType callback)
{
    return d->getBookmarksAsync(filter, callback);
}

int QnCameraBookmarksManager::getBookmarkTagsAsync(int maxTags, BookmarkTagsCallbackType callback)
{
    return d->getBookmarkTagsAsync(maxTags, callback);
}

void QnCameraBookmarksManager::addCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark");
    d->addCameraBookmark(bookmark, callback);

    emit bookmarkAdded(bookmark);
}

void QnCameraBookmarksManager::addAcknowledge(
    const QnCameraBookmark &bookmark,
    const QnUuid& eventRuleId,
    OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark");
    d->acknowledgeEvent(bookmark, eventRuleId, callback);

    emit bookmarkAdded(bookmark);
}

void QnCameraBookmarksManager::updateCameraBookmark(const QnCameraBookmark &bookmark, OperationCallbackType callback)
{
    NX_ASSERT(bookmark.isValid(), "Invalid bookmark");
    d->updateCameraBookmark(bookmark, callback);

    emit bookmarkUpdated(bookmark);
}

void QnCameraBookmarksManager::deleteCameraBookmark(const QnUuid &bookmarkId, OperationCallbackType callback)
{
    d->deleteCameraBookmark(bookmarkId, callback);

    emit bookmarkRemoved(bookmarkId);
}

QnCameraBookmarkList QnCameraBookmarksManager::cachedBookmarks(const QnCameraBookmarksQueryPtr &query) const
{
    NX_ASSERT(query);
    return d->cachedBookmarks(query);
}

void QnCameraBookmarksManager::executeQueryRemoteAsync(const QnCameraBookmarksQueryPtr &query, BookmarksCallbackType callback)
{
    NX_ASSERT(query);
    d->executeQueryRemoteAsync(query, callback);
}

QnCameraBookmarksQueryPtr QnCameraBookmarksManager::createQuery(const QnCameraBookmarkSearchFilter& filter)
{
    return d->createQuery(filter);
}
