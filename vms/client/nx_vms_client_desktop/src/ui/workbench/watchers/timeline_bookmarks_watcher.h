// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QFutureWatcher>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <camera/camera_bookmarks_manager_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnBookmarkMergeHelper;
class QnBookmarkQueriesCache;
class QnCameraBookmarkAggregation;

namespace nx::utils { class PendingOperation; }

/**
 * Caches specified count of bookmarks (5000) for all cameras. Also monitors current timeline window
 * and requests 500 bookmarks for it. Results are merged and pushed to the timeline.
 */
class QnTimelineBookmarksWatcher: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;
    using milliseconds = std::chrono::milliseconds;

public:
    QnTimelineBookmarksWatcher(QObject* parent = nullptr);
    virtual ~QnTimelineBookmarksWatcher();

public:
    QnCameraBookmarkList bookmarksAtPosition(
        milliseconds position, milliseconds msecdsPerDp = milliseconds(0)) const;

    QnCameraBookmarkList rawBookmarksAtPosition(
        const QnVirtualCameraResourcePtr& camera, qint64 positionMs) const;

    QString textFilter() const;
    void setTextFilter(const QString& value);

private:
    void onResourcesRemoved(const QnResourceList& resources);

private:
    void updateCurrentCamera();
    void updateTimelineBookmarks();

    void setTimelineBookmarks(const QnCameraBookmarkList& bookmarks);

private:
    void setCurrentCamera(const QnVirtualCameraResourcePtr& camera);

    /**
     * Create query for the given camera. It will request all bookmarks at once and periodically
     * update bookmarks near live.
     */
    QnCameraBookmarksQueryPtr ensureQueryForCamera(const QnVirtualCameraResourcePtr& camera);

    void updatePermissions();

private:
    typedef QScopedPointer<QnBookmarkQueriesCache> QnBookmarkQueriesCachePtr;
    typedef QScopedPointer<QnCameraBookmarkAggregation> QnCameraBookmarkAggregationPtr;
    typedef QSharedPointer<QnBookmarkMergeHelper> QnBookmarkMergeHelperPtr;

    const QnCameraBookmarkAggregationPtr m_aggregation;
    const QnBookmarkMergeHelperPtr m_mergeHelper;

    /** Queries for the whole timeline window. */
    const QnBookmarkQueriesCachePtr m_queriesCache;

    QnVirtualCameraResourcePtr m_currentCamera;

    QString m_textFilter;
    QFutureWatcher<QnCameraBookmark>* m_filterWatcher;
};
