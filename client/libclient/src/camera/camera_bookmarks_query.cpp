#include "camera_bookmarks_query.h"

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_resource.h>

QnCameraBookmarksQuery::QnCameraBookmarksQuery(const QnVirtualCameraResourceSet &cameras, const QnCameraBookmarkSearchFilter &filter, QObject *parent /*= nullptr*/)
    : base_type(parent)
    , m_id(QUuid::createUuid())
    , m_cameras(cameras)
    , m_filter(filter)
{
}

QnCameraBookmarksQuery::~QnCameraBookmarksQuery()
{
}

QUuid QnCameraBookmarksQuery::id() const
{
    return m_id;
}

bool QnCameraBookmarksQuery::isValid() const
{
    return !m_cameras.isEmpty() && m_filter.isValid();
}

QnVirtualCameraResourceSet QnCameraBookmarksQuery::cameras() const
{
    return m_cameras;
}

void QnCameraBookmarksQuery::setCameras(const QnVirtualCameraResourceSet &value)
{
    if (m_cameras == value)
        return;
    m_cameras = value;
    emit queryChanged();
}

void QnCameraBookmarksQuery::setCamera(const QnVirtualCameraResourcePtr &value)
{
    QnVirtualCameraResourceSet cameras;
    if (value)
        cameras << value;
    setCameras(cameras);
}

bool QnCameraBookmarksQuery::removeCamera(const QnVirtualCameraResourcePtr &value)
{
    if (!m_cameras.contains(value))
        return false;
    m_cameras.remove(value);
    emit queryChanged();
    return true;
}

QnCameraBookmarkSearchFilter QnCameraBookmarksQuery::filter() const
{
    return m_filter;
}

void QnCameraBookmarksQuery::setFilter(const QnCameraBookmarkSearchFilter &value)
{
    if (m_filter == value)
        return;
    m_filter = value;
    emit queryChanged();
}

QnCameraBookmarkList QnCameraBookmarksQuery::cachedBookmarks() const
{
    return qnCameraBookmarksManager->cachedBookmarks(toSharedPointer());
}

void QnCameraBookmarksQuery::executeRemoteAsync(BookmarksCallbackType callback)
{
    qnCameraBookmarksManager->executeQueryRemoteAsync(toSharedPointer(), callback);
}

void QnCameraBookmarksQuery::refresh()
{
    emit queryChanged();
}

QnCameraBookmarksQueryPtr QnCameraBookmarksQuery::toSharedPointer() const
{
    return QnFromThisToShared<QnCameraBookmarksQuery>::toSharedPointer();
}

