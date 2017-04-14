
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

    bool hasQuery(const QnVirtualCameraResourcePtr &camera) const;

    QnCameraBookmarksQueryPtr getOrCreateQuery(const QnVirtualCameraResourcePtr &camera);

    void removeQueryByCamera(const QnVirtualCameraResourcePtr &camera);

    // Returns true if at least one query was updated
    bool updateQueries(const QnCameraBookmarkSearchFilter &filter);

    // Returns true if query was updated
    bool updateQuery(const QnVirtualCameraResourcePtr &camera
        , const QnCameraBookmarkSearchFilter &filter);

    // Returns true if time window was updated
    bool updateQueryTimeWindow(const QnVirtualCameraResourcePtr &camera
        , qint64 startTimeMs
        , qint64 endTimeMs);

    // Returns true if filter text was updated
    bool updateQueryFilterText(const QnVirtualCameraResourcePtr &camera
        , const QString &text);

    void refreshQueries();

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