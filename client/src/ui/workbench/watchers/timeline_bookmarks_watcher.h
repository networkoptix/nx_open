
#pragma once

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/utils/timeline_bookmark_item.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QTimer;
class QnCameraBookmarkAggregation;
class QnWorkbenchBookmarksCache;
class QnBookmarkMergeHelper;

// @brief Caches bookmarks for each item on current layout and constrains
// count of them to reasonable (large) number. Gives access to merged bookmarks
// for current selected item. Gives access to bookmarks of each layout item

// TODO: #ynikitenkov TBD reasonable count of bookmarks for timeline

class QnTimelineBookmarksWatcher : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnTimelineBookmarksWatcher(QObject *parent = nullptr);

    virtual ~QnTimelineBookmarksWatcher();

public:
    QnCameraBookmarkList bookmarksAtPosition(qint64 position
        , qint64 msecdsPerDp = 0);

    QnCameraBookmarkList rawBookmarksAtPosition(
        const QnVirtualCameraResourcePtr &camera
        , qint64 positionMs);

private:
    void updateCurrentCamera();
    void onBookmarkRemoved(const QnUuid &id);
    void onBookmarksChanged(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkList &bookmarks);
    void onTimelineWindowChanged(qint64 startTimeMs
        , qint64 endTimeMs);

private:
    void setCurrentCamera(const QnVirtualCameraResourcePtr &camera);

private:
    typedef QScopedPointer<QnWorkbenchBookmarksCache> QnCurrentLayoutBookmarksCachePtr;
    typedef QScopedPointer<QnCameraBookmarkAggregation> QnCameraBookmarkAggregationPtr;
    typedef QSharedPointer<QnBookmarkMergeHelper> QnBookmarkMergeHelperPtr;
    typedef QScopedPointer<QTimer> QTimerPtr;
    const QnCurrentLayoutBookmarksCachePtr m_bookmarksCache;
    const QnBookmarkMergeHelperPtr m_mergeHelper;
    const QnCameraBookmarkAggregationPtr m_aggregation;

    QnVirtualCameraResourcePtr m_currentCamera;
    QTimerPtr m_delayedTimer;
    QElapsedTimer m_delayedUpdateCounter;
};