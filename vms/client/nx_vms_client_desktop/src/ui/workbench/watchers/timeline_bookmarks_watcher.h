#pragma once

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QElapsedTimer>

#include <recording/time_period.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QTimer;
class QnUuid;
class QnBookmarkMergeHelper;
class QnBookmarkQueriesCache;
class QnCameraBookmarksQuery;
class QnCameraBookmarkAggregation;

namespace nx::utils { class PendingOperation; }

/**
 * Caches specified count of bookmarks (5000) for all cameras. Also monitors current timeline window
 * and requests 500 bookmarks for it. Results are merged and pushed to the timeline.
 */
class QnTimelineBookmarksWatcher: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
    using milliseconds = std::chrono::milliseconds;

public:
    QnTimelineBookmarksWatcher(QObject* parent = nullptr);
    virtual ~QnTimelineBookmarksWatcher();

public:
    QnCameraBookmarkList bookmarksAtPosition(
        milliseconds position, milliseconds msecdsPerDp = milliseconds(0));

    QnCameraBookmarkList rawBookmarksAtPosition(
        const QnVirtualCameraResourcePtr& camera, qint64 positionMs);

    QString textFilter() const;
    void setTextFilter(const QString& value);

private:
    void onResourceRemoved(const QnResourcePtr& resource);

private:
    void setupTimelineWindowQuery();
    void updateCurrentCamera();
    void onBookmarkRemoved(const QnUuid& id);
    void tryUpdateTimelineBookmarks(const QnVirtualCameraResourcePtr& camera);

    void setTimelineBookmarks(const QnCameraBookmarkList& bookmarks);

private:
    void setCurrentCamera(const QnVirtualCameraResourcePtr& camera);

    /** Create static query, which will request 5000 bookmarks at once for the given camera. */
    void ensureStaticQueryForCamera(const QnVirtualCameraResourcePtr& camera);

private:
    typedef QScopedPointer<QTimer> TimerPtr;
    typedef QScopedPointer<QnBookmarkQueriesCache> QnBookmarkQueriesCachePtr;
    typedef QScopedPointer<QnCameraBookmarkAggregation> QnCameraBookmarkAggregationPtr;
    typedef QSharedPointer<QnBookmarkMergeHelper> QnBookmarkMergeHelperPtr;

    const QnCameraBookmarkAggregationPtr m_aggregation;
    const QnBookmarkMergeHelperPtr m_mergeHelper;

    /** Queries for the whole timeline window. */
    const QnBookmarkQueriesCachePtr m_queriesCache;

    /** Query for the current selected item and actual zoom level. */
    QnCameraBookmarksQueryPtr m_timelineWindowQuery;
    QnCameraBookmarkSearchFilter m_timelineWindowQueryFilter;

    QnVirtualCameraResourcePtr m_currentCamera;

    TimerPtr m_updateStaticQueriesTimer;
    QString m_textFilter;
};
