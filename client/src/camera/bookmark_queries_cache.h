
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

    QnCameraBookmarksQueryPtr getQuery(const QnVirtualCameraResourcePtr &camera);

    void removeQuery(const QnVirtualCameraResourcePtr &camera);

    void clearQueries();

    bool updateQuery(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkSearchFilter &filter);

    bool updateQueryTimeWindow(const QnVirtualCameraResourcePtr &camera
        , qint64 startTimeMs
        , qint64 endTimeMs);

    bool updateQueryFilterText(const QnVirtualCameraResourcePtr &camera
        , const QString &text);

    bool updateFilterTimeWindow(QnCameraBookmarkSearchFilter &filter
        , qint64 startTimeMs
        , qint64 endTimeMs);

    bool updateFilterText(QnCameraBookmarkSearchFilter &filter
        , const QString &text);

private:
    typedef std::function<bool (QnCameraBookmarkSearchFilter &filter)> QueryFilterCallback;
    
    bool updateDataImpl(const QnVirtualCameraResourcePtr &camera
        , const QueryFilterCallback &callback);

private:
    typedef std::map<QnVirtualCameraResourcePtr, QnCameraBookmarksQueryPtr> QueriesMap;

    const qint64 m_timeWindowMinChange;
    QueriesMap m_queries;
};