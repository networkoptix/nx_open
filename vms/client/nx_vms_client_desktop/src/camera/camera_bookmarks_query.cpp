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

QnCameraBookmarksQuery::State QnCameraBookmarksQuery::state() const
{
    return m_state;
}

void QnCameraBookmarksQuery::setState(State value)
{
    m_state = value;
}

bool QnCameraBookmarksQuery::isValid() const
{
    return m_filter.isValid();
}

bool QnCameraBookmarksQuery::active() const
{
    return m_active;
}

void QnCameraBookmarksQuery::setActive(bool active)
{
    if (m_active == active)
        return;

    m_active = active;
    emit queryChanged();
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

const QnCameraBookmarkSearchFilter& QnCameraBookmarksQuery::filter() const
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

int QnCameraBookmarksQuery::requestChunkSize() const
{
    return m_requestChunkSize;
}

void QnCameraBookmarksQuery::setRequestChunkSize(int value)
{
    NX_ASSERT(value >= 0);
    NX_ASSERT(m_filter.orderBy == QnBookmarkSortOrder::defaultOrder);

    m_requestChunkSize = value;
}

const QnCameraBookmarkList& QnCameraBookmarksQuery::cachedBookmarks() const
{
    return m_cache;
}

void QnCameraBookmarksQuery::setCachedBookmarks(QnCameraBookmarkList value)
{
    if (m_cache == value)
        return;

    m_cache = std::move(value);
    emit bookmarksChanged(m_cache);
}

void QnCameraBookmarksQuery::refresh()
{
    emit queryChanged();
}
