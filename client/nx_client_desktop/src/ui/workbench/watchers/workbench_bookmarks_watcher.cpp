
#include "workbench_bookmarks_watcher.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>

#include <camera/camera_bookmarks_query.h>
#include <camera/camera_bookmarks_manager.h>

namespace
{
    enum { kOneBookmarkLimit = 1 };

    const auto kMaxBookmarkTime = std::numeric_limits<qint64>::max();

    QnCameraBookmarksQueryPtr createBookmarkQuery(int limit)
    {
        const auto query = qnCameraBookmarksManager->createQuery();
        auto filter = query->filter();
        filter.limit = limit;
        query->setFilter(filter);
        return query;
    }
}

const qint64 QnWorkbenchBookmarksWatcher::kUndefinedTime = kMaxBookmarkTime;

QnWorkbenchBookmarksWatcher::QnWorkbenchBookmarksWatcher(QObject *parent)
    : base_type(parent)
    , m_minBookmarkTimeQuery(createBookmarkQuery(kOneBookmarkLimit))
    , m_firstBookmarkUtcTimeMs(kMaxBookmarkTime)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this
        , [this](const QnResourcePtr &resource)
    {
        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            return;

        auto currentCameras = m_minBookmarkTimeQuery->cameras();
        currentCameras.insert(camera);
        m_minBookmarkTimeQuery->setCameras(currentCameras);
    });

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this
        , [this](const QnResourcePtr &resource)
    {
        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            return;

        auto currentCameras = m_minBookmarkTimeQuery->cameras();
        currentCameras.remove(camera);
        m_minBookmarkTimeQuery->setCameras(currentCameras);
    });

    connect(m_minBookmarkTimeQuery, &QnCameraBookmarksQuery::bookmarksChanged, this
        , [this](const QnCameraBookmarkList &bookmarks)
    {
        // Bookmarks are sorted by its start time (due to Qn::EarliestFirst filter value)
        tryUpdateFirstBookmarkTime(bookmarks.empty() ? kUndefinedTime
            : bookmarks.first().startTimeMs);
    });
}

QnWorkbenchBookmarksWatcher::~QnWorkbenchBookmarksWatcher()
{
}

qint64 QnWorkbenchBookmarksWatcher::firstBookmarkUtcTimeMs() const
{
    return m_firstBookmarkUtcTimeMs;
}

void QnWorkbenchBookmarksWatcher::tryUpdateFirstBookmarkTime(qint64 utcTimeMs)
{
    if (m_firstBookmarkUtcTimeMs == utcTimeMs)
        return;

    m_firstBookmarkUtcTimeMs = utcTimeMs;
    emit firstBookmarkUtcTimeMsChanged();
}

