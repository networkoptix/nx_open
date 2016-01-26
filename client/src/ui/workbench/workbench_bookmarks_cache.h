
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

class QnWorkbenchBookmarksCache : public Connective<QObject>
    , public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnWorkbenchBookmarksCache(int maxBookmarksCount
        , const QnBookmarkSortProps &sortProp = QnBookmarkSortProps::default
        , const QnBookmarksThinOut &thinOut = QnBookmarksThinOut::kNoThinOut
        , qint64 minWindowChangeMs = 0
        , QObject *parent = nullptr);

    virtual ~QnWorkbenchBookmarksCache();

public:
    QnTimePeriod window() const;

    void setWindow(const QnTimePeriod &window);

    void setThinOut(const QnBookmarksThinOut &thinOut);

    QnCameraBookmarkList bookmarks(const QnVirtualCameraResourcePtr &camera);

private:
    void onItemAdded(QnWorkbenchItem *item);

    void onItemRemoved(QnWorkbenchItem *item);

signals:
    void bookmarksChanged(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkList &bookmarks);

    void itemAdded(QnWorkbenchItem *item);

    void itemRemoved(QnWorkbenchItem *item);

private:
    typedef QScopedPointer<QnBookmarkQueriesCache> QnBookmarkQueriesCachePtr;

    QnCameraBookmarkSearchFilter m_filter;
    QnBookmarkQueriesCachePtr m_queriesCache;
};