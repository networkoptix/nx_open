
#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/utils/timeline_bookmark_item.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnCameraBookmarkAggregation;
class QnCurrentLayoutBookmarksCache;
class QnBookmarkMergeHelper;

class QnTimelineBookmarksWatcher : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnTimelineBookmarksWatcher(QObject *parent = nullptr);

    virtual ~QnTimelineBookmarksWatcher();

public:
    QnTimelineBookmarkItemList mergedBookmarks(qint64 msecsPerDp);

    QnCameraBookmarkList bookmarksAtPosition(qint64 position
        , qint64 msecdsPerDp = 0);

private:
    void onWorkbenchCurrentWidgetChanged();
    void onBookmarksModeEnabledChanged();
    void onBookmarkRemoved(const QnUuid &id);
    void onBookmarksChanged(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkList &bookmarks);
    void onTimelineWindowChanged(qint64 startTimeMs
        , qint64 endTimeMs);

private:
    void setCurrentCamera(const QnVirtualCameraResourcePtr &camera);

private:
    typedef QScopedPointer<QnCurrentLayoutBookmarksCache> QnCurrentLayoutBookmarksCachePtr;
    typedef QScopedPointer<QnCameraBookmarkAggregation> QnCameraBookmarkAggregationPtr;
    typedef QScopedPointer<QnBookmarkMergeHelper> QnBookmarkMergeHelperPtr;

    const QnCurrentLayoutBookmarksCachePtr m_bookmarksCache;
    const QnBookmarkMergeHelperPtr m_mergeHelper;
    const QnCameraBookmarkAggregationPtr m_aggregation;

    QnVirtualCameraResourcePtr m_currentCamera;
};