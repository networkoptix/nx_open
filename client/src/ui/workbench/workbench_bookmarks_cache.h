
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
        , const QnBookmarkSortOrder &sortProp = QnBookmarkSortOrder::defaultOrder
        , const QnBookmarkSparsingOptions &sparsing = QnBookmarkSparsingOptions::kNosparsing
        , qint64 minWindowChangeMs = 0
        , QObject *parent = nullptr);

    virtual ~QnWorkbenchBookmarksCache();

public:
    QnTimePeriod window() const;

    void setWindow(const QnTimePeriod &window);

    void setsparsing(const QnBookmarkSparsingOptions &sparsing);

    QnCameraBookmarkList bookmarks(const QnVirtualCameraResourcePtr &camera);

private:
    void onItemAdded(QnWorkbenchItem *item);

    void onItemRemoved(QnWorkbenchItem *item);

    void onItemHidden(QnWorkbenchItem *item);

    void removeQueryByCamera(const QnVirtualCameraResourcePtr &camera);

    void onCurrentLayoutChanged();

signals:
    void bookmarksChanged(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkList &bookmarks);

    void itemAdded(QnWorkbenchItem *item);

    void itemRemoved(QnWorkbenchItem *item);

    void itemAboutToBeRemoved(QnWorkbenchItem *item);

private:
    typedef QScopedPointer<QnBookmarkQueriesCache> QnBookmarkQueriesCachePtr;
    typedef std::function<void ()> PostponedAction;
    typedef QLinkedList<PostponedAction> PostponedActionsList;

    QnCameraBookmarkSearchFilter m_filter;
    QnBookmarkQueriesCachePtr m_queriesCache;
    PostponedActionsList m_atLayoutChangedActions;
};