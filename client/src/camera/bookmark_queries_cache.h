
#pragma once

#include <map>

#include <QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <camera/camera_bookmarks_manager_fwd.h>

/// Class holds queries for the specified camera

class QnBookmarkQueriesCache : public QObject
{
public:
    enum { kDefaultTimeWindowMinChange = 30 * 1000 };

    QnBookmarkQueriesCache(qint64 timeWindowMinChange = kDefaultTimeWindowMinChange
        , QObject *parent = nullptr);

    virtual ~QnBookmarkQueriesCache();

    bool isQueryExists(const QnVirtualCameraResourcePtr &camera) const;

    QnCameraBookmarksQueryPtr getQuery(const QnVirtualCameraResourcePtr &camera);

    void removeQuery(const QnVirtualCameraResourcePtr &camera);

    bool updateQueries(const QnCameraBookmarkSearchFilter &filter);

    bool updateQuery(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkSearchFilter &filter);

    bool updateQueryTimeWindow(const QnVirtualCameraResourcePtr &camera
        , qint64 startTimeMs
        , qint64 endTimeMs);

    bool updateQueryFilterText(const QnVirtualCameraResourcePtr &camera
        , const QString &text);

private:
    bool updateFilterTimeWindow(QnCameraBookmarkSearchFilter &filter
        , qint64 startTimeMs
        , qint64 endTimeMs);

    bool updateFilterText(QnCameraBookmarkSearchFilter &filter
        , const QString &text);

private:
    typedef std::function<bool (QnCameraBookmarkSearchFilter &filter)> NeedUpdateFilterFunctor;

    bool updateDataImpl(const QnVirtualCameraResourcePtr &camera
        , const NeedUpdateFilterFunctor &needUpdateFunctor);

private:
    typedef std::map<QnVirtualCameraResourcePtr, QnCameraBookmarksQueryPtr> QueriesMap;

    const qint64 m_timeWindowMinChange;
    QueriesMap m_queries;
};