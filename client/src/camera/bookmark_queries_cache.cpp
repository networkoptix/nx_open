
#include "bookmark_queries_cache.h"

#include <camera/camera_bookmarks_query.h>
#include <camera/camera_bookmarks_manager.h>

QnBookmarkQueriesCache::QnBookmarkQueriesCache(qint64 timeWindowMinChange
    , QObject *parent)
    : QObject(parent)
    , m_timeWindowMinChange(timeWindowMinChange)
    , m_queries()
{}

QnBookmarkQueriesCache::~QnBookmarkQueriesCache()
{}

QnCameraBookmarksQueryPtr QnBookmarkQueriesCache::getQuery(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return QnCameraBookmarksQueryPtr();

    auto it = m_queries.find(camera);
    if (it == m_queries.end())
    {
        const auto query = qnCameraBookmarksManager->createQuery();
        query->setCamera(camera);
        it = m_queries.insert(std::make_pair(camera, query)).first;
    }

    return it->second;
}

void QnBookmarkQueriesCache::removeQuery(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return;

    auto it = m_queries.find(camera);

}

void QnBookmarkQueriesCache::clearQueries()
{
    m_queries.clear();
}

bool QnBookmarkQueriesCache::updateQuery(const QnVirtualCameraResourcePtr &camera
    , const QnCameraBookmarkSearchFilter &filter)
{
    const auto needUpdateFunctor = [this, filter](QnCameraBookmarkSearchFilter &oldFilter)
    {
        const bool textUpdated = updateFilterText(oldFilter, filter.text);
        const bool windowUpdated = updateFilterTimeWindow(oldFilter, filter.startTimeMs, filter.endTimeMs);
        return (textUpdated || windowUpdated);
    };

    return updateDataImpl(camera, needUpdateFunctor);
}

bool QnBookmarkQueriesCache::updateQueryTimeWindow(const QnVirtualCameraResourcePtr &camera
    , qint64 startTimeMs
    , qint64 endTimeMs)
{
    const auto needUpdateFunctor = [this, startTimeMs, endTimeMs](QnCameraBookmarkSearchFilter &filter)
        { return updateFilterTimeWindow(filter, startTimeMs, endTimeMs); };

    return updateDataImpl(camera, needUpdateFunctor);
}

bool QnBookmarkQueriesCache::updateQueryFilterText(const QnVirtualCameraResourcePtr &camera
    , const QString &text)
{
    const auto needUpdateFunctor = [this, text](QnCameraBookmarkSearchFilter &filter)
        { return updateFilterText(filter, text); };

    return updateDataImpl(camera, needUpdateFunctor);
}

bool QnBookmarkQueriesCache::updateDataImpl(const QnVirtualCameraResourcePtr &camera
    , const NeedUpdateFilterFunctor &needUpdateFunctor)
{
    if (!needUpdateFunctor || !camera)
        return false;

    const auto query = getQuery(camera);
    auto filter = query->filter();
    if (!needUpdateFunctor(filter))
        return false;

    query->setFilter(filter);
    return true;
}

bool QnBookmarkQueriesCache::updateFilterTimeWindow(QnCameraBookmarkSearchFilter &filter
    , qint64 startTimeMs
    , qint64 endTimeMs)
{
    const bool updateStartTime = (std::abs(filter.startTimeMs - startTimeMs) > m_timeWindowMinChange);
    const bool updateEndTime = (std::abs(filter.endTimeMs- endTimeMs) > m_timeWindowMinChange);
    if (!updateStartTime && !updateEndTime)
        return false;

    filter.startTimeMs = startTimeMs;
    filter.endTimeMs = endTimeMs;

    return true;
}

bool QnBookmarkQueriesCache::updateFilterText(QnCameraBookmarkSearchFilter &filter
    , const QString &text)
{
    if (filter.text == text)
        return false;

    filter.text = text;
    return true;
}
