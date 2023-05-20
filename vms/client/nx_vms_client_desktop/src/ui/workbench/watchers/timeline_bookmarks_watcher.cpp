// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timeline_bookmarks_watcher.h"

#include <chrono>

#include <QtCore/QTimer>

#include <camera/bookmark_queries_cache.h>
#include <camera/camera_bookmark_aggregation.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_bookmarks_query.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/utils/bookmark_merge_helper.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/camera/bookmark_helpers.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_timer.h>

using namespace nx::vms::client::desktop;
using namespace std::chrono;

namespace {

// Once in 5 minutes invalidate all loaded bookmarks.
static constexpr auto kInvalidateQueriesInterval = 5min;

// Once in 30 seconds mark all existing queries as requiring an update.
static constexpr auto kUpdateQueriesInterval = 30s;

// Request bookmark by chunks, no more than this number in a request, to load them smoothly.
static constexpr auto kRequestChunkSize = 3000;

} // namespace

QnTimelineBookmarksWatcher::QnTimelineBookmarksWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_aggregation(new QnCameraBookmarkAggregation()),
    m_mergeHelper(new QnBookmarkMergeHelper()),
    m_queriesCache(new QnBookmarkQueriesCache())
{
    connect(appContext()->unifiedResourcePool(),
        &UnifiedResourcePool::resourcesRemoved,
        this,
        &QnTimelineBookmarksWatcher::onResourcesRemoved);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this,
        &QnTimelineBookmarksWatcher::updateCurrentCamera);

    connect(navigator(), &QnWorkbenchNavigator::bookmarksModeEnabledChanged,
        [this]()
        {
            const auto bookmarksModeEnabled = navigator()->bookmarksModeEnabled();
            NX_VERBOSE(this, "Bookmark mode changed, set queries to %1", bookmarksModeEnabled);

            m_queriesCache->setQueriesActive(bookmarksModeEnabled);
            updateCurrentCamera();
        });

    navigator()->timeSlider()->setBookmarksHelper(m_mergeHelper);

    // Once a while invalidate all static queries, so bookmarks will be refreshed by request.
    auto invalidateTimer = new QTimer(this);
    invalidateTimer->setInterval(kInvalidateQueriesInterval);
    invalidateTimer->callOnTimeout(
        [this]()
        {
            NX_VERBOSE(this, "Invalidating all queries");
            m_queriesCache->clear();

            // Do not clear aggregation here, so bookmarks will be displayed until new result is
            // received.
            if (m_currentCamera)
                ensureQueryForCamera(m_currentCamera);
        });
    invalidateTimer->start();

    // Once in 30 seconds query for the currently selected camera will load potentially added
    // bookmarks near the LIVE. For the other cameras this will occur when the camera is selected.
    auto updateTimer = new QTimer(this);
    updateTimer->setInterval(kUpdateQueriesInterval);
    updateTimer->callOnTimeout(
        [this]()
        {
            if (m_currentCamera)
            {
                auto query = ensureQueryForCamera(m_currentCamera);
                if (query->state() != QnCameraBookmarksQuery::State::actual)
                    return;

                auto bookmarks = query->cachedBookmarks();
                const milliseconds timePoint = bookmarks.empty()
                    ? 0ms
                    : bookmarks.last().startTimeMs;

                NX_VERBOSE(this, "Updating query %1 tail from time %2", query->id(), timePoint);
                query->systemContext()->cameraBookmarksManager()->sendQueryTailRequest(
                    query, timePoint);
            }
        });
    updateTimer->start();
}

QnTimelineBookmarksWatcher::~QnTimelineBookmarksWatcher()
{
}

QnCameraBookmarkList QnTimelineBookmarksWatcher::bookmarksAtPosition(
    milliseconds position,
    milliseconds msecdsPerDp) const
{
    return m_mergeHelper->bookmarksAtPosition(position, msecdsPerDp);
}

QnCameraBookmarkList QnTimelineBookmarksWatcher::rawBookmarksAtPosition(
    const QnVirtualCameraResourcePtr& camera,
    qint64 positionMs) const
{
    if (camera == m_currentCamera)
        return helpers::bookmarksAtPosition(m_aggregation->bookmarkList(), positionMs);

    const auto query = m_queriesCache->getQuery(camera);
    return query
        ? helpers::bookmarksAtPosition(query->cachedBookmarks(), positionMs)
        : QnCameraBookmarkList();
}

void QnTimelineBookmarksWatcher::onResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        {
            m_queriesCache->removeQueryByCamera(camera);
            if (camera == m_currentCamera)
                setCurrentCamera({});
        }
    }
}

void QnTimelineBookmarksWatcher::updateCurrentCamera()
{
    const bool bookmarksMode = navigator()->bookmarksModeEnabled();
    const auto resourceWidget = navigator()->currentWidget();
    const auto camera = (resourceWidget && bookmarksMode
        ? resourceWidget->resource().dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr());

    setCurrentCamera(camera);
}

void QnTimelineBookmarksWatcher::updateTimelineBookmarks()
{
    if (!m_currentCamera)
    {
        m_aggregation->clear();
        m_mergeHelper->setBookmarks(QnCameraBookmarkList());
        return;
    }

    const auto query = ensureQueryForCamera(m_currentCamera);
    m_aggregation->setBookmarkList(query->cachedBookmarks());

    NX_VERBOSE(this, "Set %1 bookmarks to timeline", m_aggregation->bookmarkList().size());
    setTimelineBookmarks(m_aggregation->bookmarkList());
}

void QnTimelineBookmarksWatcher::setTimelineBookmarks(const QnCameraBookmarkList& bookmarks)
{
    if (m_textFilter.isEmpty())
    {
        NX_VERBOSE(this, "Updating timeline without text filter. %1 bookmarks.",
            bookmarks.size());
        m_mergeHelper->setBookmarks(bookmarks);
    }
    else
    {
        QnCameraBookmarkSearchFilter filter;
        filter.text = m_textFilter;

        QnCameraBookmarkList filtered(bookmarks.size());
        filtered.resize(std::distance(filtered.begin(),
            std::copy_if(bookmarks.cbegin(), bookmarks.cend(), filtered.begin(),
                [&filter](const QnCameraBookmark& bookmark)
                {
                    return filter.checkBookmark(bookmark);
                })));

        NX_VERBOSE(this, "Updating timeline with text filter. Pass %1 bookmarks of %2",
            filtered.size(), bookmarks.size());
        m_mergeHelper->setBookmarks(filtered);
    }
}

void QnTimelineBookmarksWatcher::setCurrentCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_currentCamera == camera)
        return;

    NX_VERBOSE(this, "Set current camera to %1", camera);
    m_currentCamera = camera;
    m_aggregation->clear();
    updateTimelineBookmarks();
}

QnCameraBookmarksQueryPtr QnTimelineBookmarksWatcher::ensureQueryForCamera(
    const QnVirtualCameraResourcePtr& camera)
{
    if (auto existing = m_queriesCache->getQuery(camera))
        return existing;

    NX_VERBOSE(this, "Create query for camera %1", camera);
    const auto query = m_queriesCache->getOrCreateQuery(camera);
    query->setRequestChunkSize(kRequestChunkSize);
    query->setActive(navigator()->bookmarksModeEnabled());

    connect(query.get(), &QnCameraBookmarksQuery::bookmarksChanged, this,
        [this, camera](const QnCameraBookmarkList& bookmarks)
        {
            NX_VERBOSE(this, "Camera %1 query result: %2 bookmarks", camera, bookmarks.size());
            if (m_currentCamera == camera)
                updateTimelineBookmarks();
        });
    return query;
}

QString QnTimelineBookmarksWatcher::textFilter() const
{
    return m_textFilter;
}

void QnTimelineBookmarksWatcher::setTextFilter(const QString& value)
{
    if (m_textFilter == value)
        return;

    m_textFilter = value;
    updateTimelineBookmarks();
}
