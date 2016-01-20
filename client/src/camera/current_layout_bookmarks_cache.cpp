
#include "bookmarks_cache.h"

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/current_layout_items_watcher.h>
#include <camera/bookmark_queries_cache.h>
#include <camera/camera_bookmarks_query.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <utils/common/uuid.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_item_data.h>

namespace
{
    QnCameraBookmarkSearchFilter createFilter(int maxBookmarksCount
        , Qn::BookmarkSearchStrategy strategy)
    {
        QnCameraBookmarkSearchFilter filter;
        filter.limit = maxBookmarksCount;
        filter.strategy = strategy;
        return filter;
    }

    QnVirtualCameraResourcePtr extractCamera(QnWorkbenchItem *item)
    {
        const auto layoutItemData = item->data();
        const auto id = layoutItemData.resource.id;
        return qnResPool->getResourceById<QnVirtualCameraResource>(id);
    };
};

QnCurrentLayoutBookmarksCache::QnCurrentLayoutBookmarksCache(int maxBookmarksCount
    , Qn::BookmarkSearchStrategy strategy
    , QObject *parent)

    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_filter(createFilter(maxBookmarksCount, strategy))
    , m_queriesCache(new QnBookmarkQueriesCache())
{
    const auto itemsWatcher= context()->instance<QnCurrentLayoutItemsWatcher>();
//    connect(itemsWatcher, &QnCurrentLayoutItemsWatcher::layoutChanged
//        , this, &QnCurrentLayoutBookmarksCache::onLayoutChanged);
    connect(itemsWatcher, &QnCurrentLayoutItemsWatcher::layoutAboutToBeChanged
        , this, &QnCurrentLayoutBookmarksCache::onLayoutAboutToBeChanged);
    connect(itemsWatcher, &QnCurrentLayoutItemsWatcher::itemAdded
        , this, &QnCurrentLayoutBookmarksCache::onItemAdded);
    connect(itemsWatcher, &QnCurrentLayoutItemsWatcher::itemRemoved
        , this, &QnCurrentLayoutBookmarksCache::onItemRemoved);

}

QnCurrentLayoutBookmarksCache::~QnCurrentLayoutBookmarksCache()
{
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

    // TODO add set window
}

QnCameraBookmarkList QnCurrentLayoutBookmarksCache::bookmarks(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return QnCameraBookmarkList();

    const auto query = m_queriesCache->getQuery(camera);
    return query->cachedBookmarks();
}

void QnCurrentLayoutBookmarksCache::onLayoutAboutToBeChanged()
{
    m_queriesCache->clearQueries();
}

void QnCurrentLayoutBookmarksCache::onItemAdded(QnWorkbenchItem *item)
{
    const auto camera = extractCamera(item);

    if (!camera)
        return;

    if (m_queriesCache->isQueryExists(camera))
        return; // In case of two or more items on layout

    const auto query = m_queriesCache->getQuery(camera);
    query->setFilter(m_filter);

    connect(query.data(), &QnCameraBookmarksQuery::bookmarksChanged, this
        , [this, camera](const QnCameraBookmarkList &bookmarks)
    {
        emit bookmarksChanged(camera, bookmarks);
    });
}

void QnCurrentLayoutBookmarksCache::onItemRemoved(QnWorkbenchItem *item)
{
    const auto layout = workbench()->currentLayout();
    Q_ASSERT_X(layout, Q_FUNC_INFO, "Layout should exist");

    const auto camera = extractCamera(item);
    if (!camera)
        return;

    const bool isQueryExist = m_queriesCache->isQueryExists(camera);
    Q_ASSERT_X(isQueryExist, Q_FUNC_INFO, "Camera has not been added yet");
    if (!isQueryExist)
        return;

    const bool hasSameCameras = !layout->items(camera->getUniqueId()).isEmpty();
    if (hasSameCameras)
        return;

    const auto query = m_queriesCache->getQuery(camera);
    disconnect(query, nullptr, this, nullptr);
    m_queriesCache->removeQuery(camera);
}

