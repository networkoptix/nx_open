
#include "bookmark_queries_cache.h"

#include <camera/camera_bookmarks_query.h>
#include <camera/camera_bookmarks_manager.h>

#include <utils/common/scoped_timer.h>

QnBookmarkQueriesCache::QnBookmarkQueriesCache(qint64 timeWindowMinChange
    , QObject *parent)
    : QObject(parent)
    , m_timeWindowMinChange(timeWindowMinChange)
    , m_queries()
{}

QnBookmarkQueriesCache::~QnBookmarkQueriesCache()
{}

bool QnBookmarkQueriesCache::hasQuery(const QnVirtualCameraResourcePtr &camera) const
{
    return (camera && (m_queries.find(camera) != m_queries.end()));
}

QnCameraBookmarksQueryPtr QnBookmarkQueriesCache::getOrCreateQuery(const QnVirtualCameraResourcePtr &camera)
{
    QN_LOG_TIME(Q_FUNC_INFO);

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

void QnBookmarkQueriesCache::removeQueryByCamera(const QnVirtualCameraResourcePtr &camera)
{
    if (!camera)
        return;

    m_queries.erase(camera);
}

bool QnBookmarkQueriesCache::updateQueries(const QnCameraBookmarkSearchFilter &filter)
{
    bool result = true;
    std::for_each(m_queries.begin(), m_queries.end()
        , [this, filter, &result](const QueriesMap::value_type &value)
    {
        const auto camera = value.first;
        result &= updateQuery(camera, filter);
    });
    return result;
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

    const auto query = getOrCreateQuery(camera);
    auto filter = query->filter();
    if (!needUpdateFunctor(filter))
        return false;

    query->setFilter(filter);
    return true;
}

void QnBookmarkQueriesCache::refreshQueries()
{
    for (auto queryData: m_queries)
    {
        auto &query = queryData.second;
        query->refresh();
    }
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
