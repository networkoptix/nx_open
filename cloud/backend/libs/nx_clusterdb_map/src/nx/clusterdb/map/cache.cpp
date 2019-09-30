#include "cache.h"

#include "database.h"
#include "data_manager.h"
#include "event_provider.h"

namespace nx::clusterdb::map {

Cache::Cache(Database* database):
    m_db(database)
{
    subscribeToEvents();
    initialize();
}

Cache::~Cache()
{
    m_db->eventProvider().unsubscribeFromRecordInserted(m_recordInsertedId);
    m_db->eventProvider().unsubscribeFromRecordRemoved(m_recordRemovedId);
}

std::optional<std::string> Cache::find(const std::string& key) const
{
    QnMutexLocker lock(&m_mutex);
    auto it = m_stringCache.find(key);
    if (it == m_stringCache.end())
        return std::nullopt;
    return it->second;
}

std::map<std::string, std::string> Cache::getRange(
    const std::string& lowerBound,
    const std::string& upperBound) const
{
    QnMutexLocker lock(&m_mutex);
    return {
        m_stringCache.lower_bound(lowerBound),
        m_stringCache.upper_bound(upperBound)};
}

std::map<std::string, std::string> Cache::getRange(const std::string& lowerBound) const
{
    QnMutexLocker lock(&m_mutex);
    return {m_stringCache.lower_bound(lowerBound), m_stringCache.end()};
}

std::map<std::string, std::string> Cache::getRangeWithPrefix(const std::string& prefix) const
{
    auto upperBound = calculateUpperBound(prefix);
    return upperBound ? getRange(prefix, *upperBound) : getRange(prefix);
}

void Cache::initialize()
{
    std::promise<void> done;
    m_db->dataManager().getAll(
        [this, &done](
            ResultCode resultCode,
            std::map<std::string/*key*/, std::string /*value*/> map)
        {
            if (resultCode != ResultCode::ok)
            {
                NX_WARNING(this, "Failed to initialize cache: %1", toString(resultCode));
                done.set_value();
                return;
            }

            {
                QnMutexLocker lock(&m_mutex);
                m_stringCache = std::move(map);
            }

            done.set_value();
        });

    done.get_future().get();
}

void Cache::subscribeToEvents()
{
    m_db->eventProvider().subscribeToRecordInserted(
        [this](
            nx::sql::QueryContext* /*queryContext*/,
            const std::string& key,
            std::string value)
        {
            QnMutexLocker lock(&m_mutex);
            m_stringCache[key] = std::move(value);
        },
        &m_recordInsertedId);

    m_db->eventProvider().subscribeToRecordRemoved(
        [this](
            nx::sql::QueryContext* /*queryContext*/,
            const std::string& key)
        {
            QnMutexLocker lock(&m_mutex);
            m_stringCache.erase(key);
        },
        &m_recordRemovedId);
}

} // namespace nx::clusterdb::map