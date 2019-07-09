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

void Cache::initialize()
{
    m_db->dataManager().getAll(
        [this](ResultCode resultCode, std::map<std::string/*key*/, std::string /*value*/> map)
        {
            if (resultCode != ResultCode::ok)
            {
                NX_WARNING(this, "Failed to initialize cache: %1", toString(resultCode));
                return;
            }

            QnMutexLocker lock(&m_mutex);
            m_stringCache = std::move(map);
        });
}

void Cache::subscribeToEvents()
{
    m_db->eventProvider().subscribeToRecordInserted(
        [this](
            nx::sql::QueryContext* /*queryContext*/,
            std::string key,
            std::string value)
        {
            QnMutexLocker lock(&m_mutex);
            m_stringCache[key] = value;
        },
        &m_recordInsertedId);

    m_db->eventProvider().subscribeToRecordRemoved(
        [this](
            nx::sql::QueryContext* /*queryContext*/,
            std::string key)
        {
            QnMutexLocker lock(&m_mutex);
            m_stringCache.erase(key);
        },
        &m_recordRemovedId);
}

} // namespace nx::clusterdb::map