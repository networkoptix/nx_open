
#include "current_layout_bookmarks_cache.h"

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/current_layout_items_watcher.h>
#include <ui/utils/workbench_item_helper.h>
#include <camera/bookmark_queries_cache.h>
#include <camera/camera_bookmarks_query.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <utils/common/uuid.h>
#include <core/resource/camera_resource.h>

namespace
{
    QnCameraBookmarkSearchFilter createFilter(int maxBookmarksCount
        , const QnBookmarkSortProps &sortProps
        , const QnBookmarksThinOut &thinOut)
    {
        QnCameraBookmarkSearchFilter filter;
        filter.limit = maxBookmarksCount;
        filter.sortProps= sortProps;
        filter.thinOut = thinOut;
        return filter;
    }
};

QnCurrentLayoutBookmarksCache::QnCurrentLayoutBookmarksCache(int maxBookmarksCount
    , const QnBookmarkSortProps &sortProp
    , const QnBookmarksThinOut &thinOut
    , qint64 minWindowChangeMs
    , QObject *parent)

    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_filter(createFilter(maxBookmarksCount, sortProp, thinOut))
    , m_queriesCache(new QnBookmarkQueriesCache(minWindowChangeMs))
{
    const auto itemsWatcher= context()->instance<QnCurrentLayoutItemsWatcher>();
    connect(itemsWatcher, &QnCurrentLayoutItemsWatcher::itemAdded
        , this, &QnCurrentLayoutBookmarksCache::onItemAdded);
    connect(itemsWatcher, &QnCurrentLayoutItemsWatcher::itemRemoved
        , this, &QnCurrentLayoutBookmarksCache::onItemRemoved);

}

QnCurrentLayoutBookmarksCache::~QnCurrentLayoutBookmarksCache()
{
}

QnTimePeriod QnCurrentLayoutBookmarksCache::window() const
{
    return QnTimePeriod::fromInterval(m_filter.startTimeMs, m_filter.endTimeMs);
}

void QnCurrentLayoutBookmarksCache::setWindow(const QnTimePeriod &window)
{
    if ((m_filter.startTimeMs == window.startTimeMs)
        && (m_filter.endTimeMs == window.endTimeMs()))
    {
        return;
    }

    m_filter.startTimeMs = window.startTimeMs;
    m_filter.endTimeMs = window.endTimeMs();
    m_queriesCache->updateQueries(m_filter);
}

void QnCurrentLayoutBookmarksCache::setThinOut(const QnBookmarksThinOut &thinOut)
{
    if (m_filter.thinOut == thinOut)
        return;

    m_filter.thinOut = thinOut;
    m_queriesCache->updateQueries(m_filter);
}

QnCameraBookmarkList QnCurrentLayoutBookmarksCache::bookmarks(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return QnCameraBookmarkList();

    const auto query = m_queriesCache->getOrCreateQuery(camera);
    return query->cachedBookmarks();
}

void QnCurrentLayoutBookmarksCache::onItemAdded(QnWorkbenchItem *item)
{
    emit itemAdded(item);

    const auto camera = helpers::extractCameraResource(item);

    if (!camera)
        return;

    if (m_queriesCache->hasQuery(camera))
        return; // In case of two or more items on layout

    const auto query = m_queriesCache->getOrCreateQuery(camera);
    query->setFilter(m_filter);

    connect(query.data(), &QnCameraBookmarksQuery::bookmarksChanged, this
        , [this, camera](const QnCameraBookmarkList &bookmarks)
    {
        emit bookmarksChanged(camera, bookmarks);
    });
}

void QnCurrentLayoutBookmarksCache::onItemRemoved(QnWorkbenchItem *item)
{
    emit itemRemoved(item);
    const auto layout = workbench()->currentLayout();
    Q_ASSERT_X(layout, Q_FUNC_INFO, "Layout should exist");

    const auto camera = helpers::extractCameraResource(item);
    if (!camera)
        return;

    const bool isQueryExist = m_queriesCache->hasQuery(camera);
    Q_ASSERT_X(isQueryExist, Q_FUNC_INFO, "Camera has not been added yet");
    if (!isQueryExist)
        return;

    const bool hasSameCameras = !layout->items(camera->getUniqueId()).isEmpty();
    if (hasSameCameras)
        return;

    const auto query = m_queriesCache->getOrCreateQuery(camera);
    disconnect(query, nullptr, this, nullptr);
    m_queriesCache->removeQueryByCamera(camera);
}

