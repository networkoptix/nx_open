// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "bookmark_queries_cache.h"

#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx::vms::client::desktop;

void QnBookmarkQueriesCache::clear()
{
    m_queries.clear();
}

bool QnBookmarkQueriesCache::hasQuery(const QnVirtualCameraResourcePtr& camera) const
{
    return camera && (m_queries.find(camera) != m_queries.end());
}

QnCameraBookmarksQueryPtr QnBookmarkQueriesCache::getQuery(
    const QnVirtualCameraResourcePtr& camera) const
{
    auto it = m_queries.find(camera);
    if (it != m_queries.end())
        return it->second;

    return {};
}

QnCameraBookmarksQueryPtr QnBookmarkQueriesCache::getOrCreateQuery(
    const QnVirtualCameraResourcePtr& camera)
{
    if (!camera || !camera->systemContext())
        return QnCameraBookmarksQueryPtr();

    auto it = m_queries.find(camera);
    if (it == m_queries.end())
    {
        auto systemContext = SystemContext::fromResource(camera);
        const auto query = systemContext->cameraBookmarksManager()->createQuery();
        query->setCamera(camera->getId());
        it = m_queries.insert(std::make_pair(camera, query)).first;
    }

    return it->second;
}

void QnBookmarkQueriesCache::removeQueryByCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera)
        return;

    m_queries.erase(camera);
}

void QnBookmarkQueriesCache::setQueriesActive(bool active)
{
    for (const auto& [_, query]: m_queries)
        query->setActive(active);
}
