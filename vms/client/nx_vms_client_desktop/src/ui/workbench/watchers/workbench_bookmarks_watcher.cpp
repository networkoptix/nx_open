#include "workbench_bookmarks_watcher.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>

#include <camera/camera_bookmarks_query.h>
#include <camera/camera_bookmarks_manager.h>

namespace
{
enum { kOneBookmarkLimit = 1 };

QnCameraBookmarksQueryPtr createBookmarkQuery(int limit)
{
    const auto query = qnCameraBookmarksManager->createQuery();
    auto filter = query->filter();
    filter.limit = limit;
    query->setFilter(filter);
    return query;
}
} // namespace

QnWorkbenchBookmarksWatcher::QnWorkbenchBookmarksWatcher(QObject *parent):
    base_type(parent),
    m_minBookmarkTimeQuery(createBookmarkQuery(kOneBookmarkLimit)),
    m_firstBookmarkUtcTime(kUndefinedTime)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr &resource)
        {
            const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
            if (!camera)
                return;

            auto currentCameras = m_minBookmarkTimeQuery->cameras();
            currentCameras.insert(camera);
            m_minBookmarkTimeQuery->setCameras(currentCameras);
        });

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr &resource)
        {
            const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
            if (!camera)
                return;

            auto currentCameras = m_minBookmarkTimeQuery->cameras();
            currentCameras.remove(camera);
            m_minBookmarkTimeQuery->setCameras(currentCameras);
        });

    connect(m_minBookmarkTimeQuery, &QnCameraBookmarksQuery::bookmarksChanged, this,
        [this](const QnCameraBookmarkList &bookmarks)
        {
            // Bookmarks are sorted by its start time (due to Qn::EarliestFirst filter value).
            tryUpdateFirstBookmarkTime(bookmarks.empty()
                ? kUndefinedTime
                : bookmarks.first().startTimeMs);
        });
}

QnWorkbenchBookmarksWatcher::~QnWorkbenchBookmarksWatcher()
{
}

std::chrono::milliseconds QnWorkbenchBookmarksWatcher::firstBookmarkUtcTime() const
{
    return m_firstBookmarkUtcTime;
}

void QnWorkbenchBookmarksWatcher::tryUpdateFirstBookmarkTime(milliseconds utcTime)
{
    if (m_firstBookmarkUtcTime == utcTime)
        return;

    m_firstBookmarkUtcTime = utcTime;
    emit firstBookmarkUtcTimeChanged();
}

