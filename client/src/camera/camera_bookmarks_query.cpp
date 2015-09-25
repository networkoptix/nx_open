#include "camera_bookmarks_query.h"

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_resource.h>


QnCameraBookmarksQuery::QnCameraBookmarksQuery(QObject *parent /*= nullptr*/)
    : base_type(parent)
    , m_autoUpdate(false)
    , m_cameras()
    , m_filter()
{}

QnCameraBookmarksQuery::QnCameraBookmarksQuery(const QnVirtualCameraResourceList &cameras, const QnCameraBookmarkSearchFilter &filter, QObject *parent /*= nullptr*/)
    : base_type(parent)
    , m_autoUpdate(false)
    , m_cameras(cameras)
    , m_filter(filter)
{}

QnCameraBookmarksQuery::~QnCameraBookmarksQuery() {
    setAutoUpdate(false);    
}

bool QnCameraBookmarksQuery::isValid() const {
    return !m_cameras.isEmpty() && m_filter.isValid();
}

bool QnCameraBookmarksQuery::autoUpdate() const {
    return m_autoUpdate;
}

void QnCameraBookmarksQuery::setAutoUpdate(bool value) {
    if (m_autoUpdate == value)
        return;
    m_autoUpdate = value;
    if (value)
        qnCameraBookmarksManager->registerAutoUpdateQuery(toSharedPointer());
    else
        qnCameraBookmarksManager->unregisterAutoUpdateQuery(toSharedPointer());
}

QnVirtualCameraResourceList QnCameraBookmarksQuery::cameras() const {
    return m_cameras;
}

void QnCameraBookmarksQuery::setCameras(const QnVirtualCameraResourceList &value) {
    if (m_cameras == value)
        return;
    m_cameras = value;
    emit queryChanged();
}

QnCameraBookmarkSearchFilter QnCameraBookmarksQuery::filter() const {
    return m_filter;
}

void QnCameraBookmarksQuery::setFilter(const QnCameraBookmarkSearchFilter &value) {
    if (m_filter == value)
        return;
    m_filter = value;
    emit queryChanged();
}

QnCameraBookmarkList QnCameraBookmarksQuery::executeLocal() const {
    return qnCameraBookmarksManager->executeQueryLocal(toSharedPointer());
}

void QnCameraBookmarksQuery::executeRemoteAsync(BookmarksCallbackType callback) {
    qnCameraBookmarksManager->executeQueryRemoteAsync(toSharedPointer(), callback);
}

QnCameraBookmarksQueryPtr QnCameraBookmarksQuery::toSharedPointer() const {
    return QnFromThisToShared<QnCameraBookmarksQuery>::toSharedPointer();
}

