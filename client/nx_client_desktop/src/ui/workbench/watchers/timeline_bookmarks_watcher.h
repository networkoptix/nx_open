#pragma once

#include <chrono>

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

#include <recording/time_period.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QnUuid;
class QnBookmarkMergeHelper;
class QnBookmarkQueriesCache;
class QnCameraBookmarksQuery;
class QnCameraBookmarkAggregation;

namespace nx { namespace utils { class PendingOperation; } }

// @brief Caches specified count of bookmarks for all cameras.
// Gives access to merged bookmarks for current selected item.
// Gives access to bookmarks of each layout item.
class QnTimelineBookmarksWatcher : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

    using milliseconds = std::chrono::milliseconds;

public:
    QnTimelineBookmarksWatcher(QObject *parent = nullptr);

    virtual ~QnTimelineBookmarksWatcher();

public:
    QnCameraBookmarkList bookmarksAtPosition(milliseconds position,
        milliseconds msecdsPerDp = milliseconds(0));

    QnCameraBookmarkList rawBookmarksAtPosition(
        const QnVirtualCameraResourcePtr &camera,
        qint64 positionMs);

    QString textFilter() const;
    void setTextFilter(const QString& value);

private:
    void onResourceRemoved(const QnResourcePtr &resource);

private:
    void updateCurrentCamera();
    void onBookmarkRemoved(const QnUuid &id);
    void tryUpdateTimelineBookmarks(const QnVirtualCameraResourcePtr &camera);
    void onTimelineWindowChanged(milliseconds startTimeMs, milliseconds endTimeMs);

    void setTimelineBookmarks(const QnCameraBookmarkList& bookmarks);

private:
    void setCurrentCamera(const QnVirtualCameraResourcePtr &camera);
    void ensureQueryForCamera(const QnVirtualCameraResourcePtr &camera);

private:
    typedef QScopedPointer<QTimer> TimerPtr;
    typedef QScopedPointer<QnBookmarkQueriesCache> QnBookmarkQueriesCachePtr;
    typedef QScopedPointer<QnCameraBookmarkAggregation> QnCameraBookmarkAggregationPtr;
    typedef QSharedPointer<QnBookmarkMergeHelper> QnBookmarkMergeHelperPtr;

    const QnCameraBookmarkAggregationPtr m_aggregation;
    const QnBookmarkMergeHelperPtr m_mergeHelper;
    const QnBookmarkQueriesCachePtr m_queriesCache;     // Holds queries for hole timeline window
    QnCameraBookmarksQueryPtr m_timelineQuery;     // Holds query for current selected item
    QnVirtualCameraResourcePtr m_currentCamera;

    TimerPtr m_updateStaticQueriesTimer;
    nx::utils::PendingOperation* m_updateQueryOperation;
    QnCameraBookmarkSearchFilter m_timlineFilter;
    QString m_textFilter;
};
