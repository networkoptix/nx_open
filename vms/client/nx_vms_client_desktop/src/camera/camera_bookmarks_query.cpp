// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bookmarks_query.h"

#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx::vms::client::desktop;

QnCameraBookmarksQuery::QnCameraBookmarksQuery(
    SystemContext* systemContext,
    const QnCameraBookmarkSearchFilter& filter,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    m_id(QUuid::createUuid()),
    m_filter(filter)
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
    return m_filter.isValid();
}

const std::set<QnUuid>& QnCameraBookmarksQuery::cameras() const
{
    return m_filter.cameras;
}

void QnCameraBookmarksQuery::setCameras(const std::set<QnUuid>& value)
{
    if (m_filter.cameras == value)
        return;
    m_filter.cameras = value;
    emit queryChanged();
}

void QnCameraBookmarksQuery::setCamera(const QnUuid& value)
{
    setCameras({value});
}

bool QnCameraBookmarksQuery::addCamera(const QnUuid& value)
{
    if (!NX_ASSERT(!value.isNull()))
        return false;

    if (m_filter.cameras.contains(value))
        return false;

    m_filter.cameras.insert(value);
    emit queryChanged();
    return true;
}

bool QnCameraBookmarksQuery::removeCamera(const QnUuid& value)
{
    if (!m_filter.cameras.contains(value))
        return false;
    m_filter.cameras.erase(value);
    emit queryChanged();
    return true;
}

QnCameraBookmarkSearchFilter QnCameraBookmarksQuery::filter() const
{
    return m_filter;
}

void QnCameraBookmarksQuery::setFilter(const QnCameraBookmarkSearchFilter& value)
{
    if (m_filter == value)
        return;
    m_filter = value;
    emit queryChanged();
}

QnCameraBookmarkList QnCameraBookmarksQuery::cachedBookmarks() const
{
    return systemContext()->cameraBookmarksManager()->cachedBookmarks(toSharedPointer());
}

void QnCameraBookmarksQuery::executeRemoteAsync(BookmarksCallbackType callback)
{
    systemContext()->cameraBookmarksManager()->executeQueryRemoteAsync(toSharedPointer(), callback);
}

void QnCameraBookmarksQuery::refresh()
{
    emit queryChanged();
}

QnCameraBookmarksQueryPtr QnCameraBookmarksQuery::toSharedPointer() const
{
    return QnFromThisToShared<QnCameraBookmarksQuery>::toSharedPointer();
}
