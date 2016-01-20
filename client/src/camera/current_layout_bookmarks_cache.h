
#pragma once

#include <QtCore/QObject>

#include <recording/time_period.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QnWorkbenchItem;
class QnBookmarkQueriesCache;

class QnCurrentLayoutBookmarksCache : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnCurrentLayoutBookmarksCache(int maxBookmarksCount
        , Qn::BookmarkSearchStrategy strategy
        , QObject *parent = nullptr);

    virtual ~QnCurrentLayoutBookmarksCache();

public:
    void setWindow(const QnTimePeriod &window);

    QnCameraBookmarkList bookmarks(const QnVirtualCameraResourcePtr &camera);

private:
    void onItemAdded(QnWorkbenchItem *item);

    void onItemRemoved(QnWorkbenchItem *item);

signals:
    void bookmarksChanged(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkList &bookmarks);
private:
    typedef QScopedPointer<QnBookmarkQueriesCache> QnBookmarkQueriesCachePtr;

    QnCameraBookmarkSearchFilter m_filter;
    QnBookmarkQueriesCachePtr m_queriesCache;
};