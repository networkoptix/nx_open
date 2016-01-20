
#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/utils/timeline_bookmark_item.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

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
    void onCameraBookmarksChanged();
    void onBookmarksChanged(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkList &bookmarks);

private:
    void setCurrentCamera(const QnVirtualCameraResourcePtr &camera);

private:
    typedef QScopedPointer<QnCurrentLayoutBookmarksCache> QnCurrentLayoutBookmarksCachePtr;
    typedef QScopedPointer<QnBookmarkMergeHelper> QnBookmarkMergeHelperPtr;

    QnCurrentLayoutBookmarksCachePtr m_bookmarksCache;
    QnBookmarkMergeHelperPtr m_mergeHelper;
    QnVirtualCameraResourcePtr m_currentCamera;
};