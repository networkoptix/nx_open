
#include "workbench_bookmarks_cache.h"

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
        , const QnBookmarkSortOrder &orderBy
        , const QnBookmarkSparsingOptions &sparsing)
    {
        QnCameraBookmarkSearchFilter filter;
        filter.limit = maxBookmarksCount;
        filter.orderBy= orderBy;
        filter.sparsing = sparsing;
        return filter;
    }
};

QnWorkbenchBookmarksCache::QnWorkbenchBookmarksCache(int maxBookmarksCount
    , const QnBookmarkSortOrder &sortProp
    , const QnBookmarkSparsingOptions &sparsing
    , qint64 minWindowChangeMs
    , QObject *parent)

    : base_type(parent)
    , QnWorkbenchContextAware(parent)

    , m_filter(createFilter(maxBookmarksCount, sortProp, sparsing))
    , m_queriesCache(new QnBookmarkQueriesCache(minWindowChangeMs))
    , m_atLayoutChangedActions()
{
    const auto itemsWatcher= context()->instance<QnCurrentLayoutItemsWatcher>();
    connect(itemsWatcher, &QnCurrentLayoutItemsWatcher::itemAdded
        , this, &QnWorkbenchBookmarksCache::onItemAdded);
    connect(itemsWatcher, &QnCurrentLayoutItemsWatcher::itemRemoved
        , this, &QnWorkbenchBookmarksCache::onItemRemoved);
    connect(itemsWatcher, &QnCurrentLayoutItemsWatcher::itemHidden
        , this, &QnWorkbenchBookmarksCache::onItemAboutToBeRemoved);
    connect(itemsWatcher, &QnCurrentLayoutItemsWatcher::layoutChanged
        , this, &QnWorkbenchBookmarksCache::onCurrentLayoutChanged);

    connect(qnResPool, &QnResourcePool::resourceRemoved, this
        , [this](const QnResourcePtr &resource)
    {
        removeQueryByCamera(resource.dynamicCast<QnVirtualCameraResource>());
    });
}

QnWorkbenchBookmarksCache::~QnWorkbenchBookmarksCache()
{
}

QnTimePeriod QnWorkbenchBookmarksCache::window() const
{
    return QnTimePeriod::fromInterval(m_filter.startTimeMs, m_filter.endTimeMs);
}

void QnWorkbenchBookmarksCache::setWindow(const QnTimePeriod &window)
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

void QnWorkbenchBookmarksCache::setsparsing(const QnBookmarkSparsingOptions &sparsing)
{
    if (m_filter.sparsing == sparsing)
        return;

    m_filter.sparsing = sparsing;
    m_queriesCache->updateQueries(m_filter);
}

QnCameraBookmarkList QnWorkbenchBookmarksCache::bookmarks(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return QnCameraBookmarkList();

    const auto query = m_queriesCache->getOrCreateQuery(camera);
    return query->cachedBookmarks();
}

void QnWorkbenchBookmarksCache::onItemAdded(QnWorkbenchItem *item)
{
    emit itemAdded(item);

    const auto camera = helpers::extractCameraResource(item);

    if (!camera)
        return;

    const auto query = m_queriesCache->getOrCreateQuery(camera);
    if (query->filter() == m_filter)
        return; // We have initialized query already

    query->setFilter(m_filter);
    connect(query.data(), &QnCameraBookmarksQuery::bookmarksChanged, this
        , [this, camera](const QnCameraBookmarkList &bookmarks)
    {
        emit bookmarksChanged(camera, bookmarks);
    });
}

void QnWorkbenchBookmarksCache::onItemRemoved(QnWorkbenchItem *item)
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

    removeQueryByCamera(camera);
}

void QnWorkbenchBookmarksCache::onItemAboutToBeRemoved(QnWorkbenchItem *item)
{
    emit itemAboutToBeRemoved(item);

    const auto camera = helpers::extractCameraResource(item);
    if (!camera)
        return;

    auto removeItemAction = [this, camera]()
    {
        const auto layout = workbench()->currentLayout();
        const auto hasItemsWithThisCamera = (layout && !layout->items(camera->getUniqueId()).isEmpty());
        if (hasItemsWithThisCamera)
            return; // Do not remove query for this camera - it holds (possibly) some bookmarks for visible period

        removeQueryByCamera(camera);
    };

    m_atLayoutChangedActions.append(removeItemAction);
}

void QnWorkbenchBookmarksCache::onCurrentLayoutChanged()
{
    for (const auto action: m_atLayoutChangedActions)
        action();

    m_atLayoutChangedActions.clear();
}


void QnWorkbenchBookmarksCache::removeQueryByCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return;

    const auto query = m_queriesCache->getOrCreateQuery(camera);
    disconnect(query, nullptr, this, nullptr);
    m_queriesCache->removeQueryByCamera(camera);
}


