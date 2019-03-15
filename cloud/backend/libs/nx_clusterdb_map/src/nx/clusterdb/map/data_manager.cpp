#include "data_manager.h"

#include <nx/sql/query_context.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/clusterdb/engine/synchronization_engine.h>

#include "key_value_pair.h"
#include "dao/key_value_dao.h"
#include "event_provider.h"

namespace nx::clusterdb::map {

namespace {

ResultCode toResultCode(nx::sql::DBResult dbResult)
{
    using namespace nx::sql;
    switch (dbResult)
    {
        case DBResult::ok:
            return ResultCode::ok;
        case DBResult::notFound:
            return ResultCode::notFound;
        case DBResult::logicError:
            return ResultCode::logicError;
        default:
            return ResultCode::unknownError;
    }
}

template<typename OptionalType>
nx::sql::DBResult filterDbResult(
    const std::optional<OptionalType>& optional,
    nx::sql::DBResult result)
{
    return !optional.has_value() && result == nx::sql::DBResult::ok
        ? nx::sql::DBResult::notFound
        : result;
}

template<typename OptionalType>
OptionalType getValue(const std::optional<OptionalType>& optional)
{
    return optional.has_value() ? *optional : OptionalType();
}

std::optional<std::string> calculateUpperBound(const std::string& keyPrefix)
{
    if (!keyPrefix.empty())
    {
        std::string upperBound = keyPrefix;
        for (auto rit = upperBound.rbegin(); rit != upperBound.rend(); ++rit)
        {
            if (++(*rit) != 0)
                return upperBound;
        }
    }
    return std::nullopt;
}

} // namespace

DataManager::DataManager(
    nx::clusterdb::engine::SynchronizationEngine* synchronizationEngine,
    nx::sql::AsyncSqlQueryExecutor* queryExecutor,
    const std::string& clusterId,
    EventProvider* eventProvider)
    :
    m_syncEngine(synchronizationEngine),
    m_queryExecutor(queryExecutor),
    m_clusterId(clusterId),
    m_eventProvider(eventProvider)
{
    m_syncEngine->incomingCommandDispatcher()
        .registerCommandHandler<command::SaveKeyValuePair>(
            [this](auto&&... args)
            {
                insertOrUpdateReceivedRecord(std::forward<decltype(args)>(args)...);
                return nx::sql::DBResult::ok;
            });

    m_syncEngine->incomingCommandDispatcher()
        .registerCommandHandler<command::RemoveKeyValuePair>(
            [this](auto&&... args)
            {
                removeReceivedRecord(std::forward<decltype(args)>(args)...);
                return nx::sql::DBResult::ok;
            });
}

DataManager::~DataManager()
{
    m_syncEngine->incomingCommandDispatcher()
        .removeHandler<command::SaveKeyValuePair>();

    m_syncEngine->incomingCommandDispatcher()
        .removeHandler<command::RemoveKeyValuePair>();
}

void DataManager::insertOrUpdate(
    const std::string& key,
    const std::string& value,
    UpdateCompletionHandler completionHandler)
{
    if (key.empty())
        return completionHandler(ResultCode::logicError);

    m_syncEngine->transactionLog().startDbTransaction(
        m_clusterId,
        [this, key, value](nx::sql::QueryContext* queryContext)
        {
            insertToOrUpdateDb(queryContext, key, value);
            return nx::sql::DBResult::ok;
        },
        [completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult)
        {
            completionHandler(toResultCode(dbResult));
        });
}

void DataManager::remove(
    const std::string& key,
    UpdateCompletionHandler completionHandler)
{
    if (key.empty())
        return completionHandler(ResultCode::logicError);

    m_syncEngine->transactionLog().startDbTransaction(
        m_clusterId,
        [this, key](nx::sql::QueryContext* queryContext)
        {
            removeFromDb(queryContext, key);
            return nx::sql::DBResult::ok;
        },
        [completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult)
        {
            completionHandler(toResultCode(dbResult));
        });
}

void DataManager::get(
    const std::string& key,
    LookupCompletionHandler completionHandler)
{
    if (key.empty())
        return completionHandler(ResultCode::logicError, std::string());

    auto sharedValue = std::make_shared<std::optional<std::string>>();

    m_queryExecutor->executeSelect(
        [this, key, sharedValue](nx::sql::QueryContext* queryContext) mutable
        {
            *sharedValue = getFromDb(queryContext, key);
            return nx::sql::DBResult::ok;
        },
        [sharedValue, completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult) mutable
        {
            completionHandler(
                toResultCode(filterDbResult(*sharedValue, dbResult)),
                getValue(*sharedValue));
        }
    );
}

void DataManager::lowerBound(const std::string& key, LookupCompletionHandler completionHandler)
{
    if (key.empty())
        return completionHandler(ResultCode::logicError, std::string());

    auto sharedKey = std::make_shared<std::optional<std::string>>();

    m_queryExecutor->executeSelect(
        [this, key, sharedKey](nx::sql::QueryContext* queryContext) mutable
        {
            *sharedKey = getLowerBoundFromDb(queryContext, key);
            return nx::sql::DBResult::ok;
        },
        [sharedKey, completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult)
        {
            completionHandler(
                toResultCode(filterDbResult(*sharedKey, dbResult)),
                getValue(*sharedKey));
        });
}

void DataManager::upperBound(const std::string& key, LookupCompletionHandler completionHandler)
{
    if (key.empty())
        return completionHandler(ResultCode::logicError, std::string());

    auto sharedKey = std::make_shared<std::optional<std::string>>();

    m_queryExecutor->executeSelect(
        [this, key, sharedKey](nx::sql::QueryContext* queryContext) mutable
        {
            *sharedKey = getUpperBoundFromDb(queryContext, key);
            return nx::sql::DBResult::ok;
        },
        [sharedKey, completionHandler = std::move(completionHandler)](
                nx::sql::DBResult dbResult)
        {
            completionHandler(
                toResultCode(filterDbResult(*sharedKey, dbResult)),
                getValue(*sharedKey));
        });
}

void DataManager::getRange(
    const std::string& keyLowerBound,
    const std::string& keyUpperBound,
    GetRangeCompletionHandler completionHandler)
{
    if (keyLowerBound > keyUpperBound || keyLowerBound.empty() || keyUpperBound.empty())
        return completionHandler(ResultCode::logicError, std::map<std::string, std::string>());

    auto sharedPairs = std::make_shared<std::optional<std::map<std::string, std::string>>>();

    m_queryExecutor->executeSelect(
        [this, keyLowerBound, keyUpperBound, sharedPairs](
            nx::sql::QueryContext* queryContext) mutable
        {
            *sharedPairs = getRangeFromDb(queryContext, keyLowerBound, keyUpperBound);
            return nx::sql::DBResult::ok;
        },
        [sharedPairs, completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult)
        {
            completionHandler(
                toResultCode(filterDbResult(*sharedPairs, dbResult)),
                getValue(*sharedPairs));
        });
}

void DataManager::getRange(
    const std::string& keyLowerBound,
    GetRangeCompletionHandler completionHandler)
{
    if (keyLowerBound.empty())
        return completionHandler(ResultCode::logicError, std::map<std::string, std::string>());

    auto sharedPairs = std::make_shared<std::optional<std::map<std::string, std::string>>>();

    m_queryExecutor->executeSelect(
        [this, keyLowerBound, sharedPairs](
            nx::sql::QueryContext* queryContext) mutable
        {
            *sharedPairs = getRangeFromDb(queryContext, keyLowerBound);
            return nx::sql::DBResult::ok;
        },
        [sharedPairs, completionHandler = std::move(completionHandler)](
                nx::sql::DBResult dbResult)
        {
            completionHandler(
                toResultCode(filterDbResult(*sharedPairs, dbResult)),
                getValue(*sharedPairs));
        });
}

void DataManager::getRangeWithPrefix(
    const std::string& keyPrefix,
    GetRangeCompletionHandler completionHandler)
{
    if (keyPrefix.empty())
        return completionHandler(ResultCode::logicError, std::map<std::string, std::string>());

    auto keyPrefixUpperBound = calculateUpperBound(keyPrefix);

    if (keyPrefixUpperBound.has_value())
        getRange(keyPrefix, *keyPrefixUpperBound, std::move(completionHandler));
    else
        getRange(keyPrefix, std::move(completionHandler));
}

void DataManager::insertToOrUpdateDb(
    nx::sql::QueryContext* queryContext,
    const std::string& key,
    const std::string& value)
{
    m_keyValueDao.insertOrUpdate(queryContext, key, value);
    m_eventProvider->notifyRecordInserted(queryContext, key, value);

    m_syncEngine->transactionLog()
        .generateTransactionAndSaveToLog<command::SaveKeyValuePair>(
            queryContext, m_clusterId, KeyValuePair{key, value});
}

std::optional<std::string> DataManager::getFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
   return m_keyValueDao.get(queryContext, key);
}


std::optional<std::string> DataManager::getLowerBoundFromDb(
    nx::sql::QueryContext * queryContext,
    const std::string& key)
{
    return m_keyValueDao.lowerBound(queryContext, key);
}


std::optional<std::string> DataManager::getUpperBoundFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    return m_keyValueDao.upperBound(queryContext, key);
}

std::optional<std::map<std::string, std::string>> DataManager::getRangeFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& keyLowerBound,
    const std::string& keyUpperBound)
{
    auto range = m_keyValueDao.getRange(queryContext, keyLowerBound, keyUpperBound);
    return range.empty() ? std::nullopt : std::optional<decltype(range)>(range);
}

std::optional<std::map<std::string, std::string>> DataManager::getRangeFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& keyLowerBound)
{
    auto range = m_keyValueDao.getRange(queryContext, keyLowerBound);
    return range.empty() ? std::nullopt : std::optional<decltype(range)>(range);
}

void DataManager::removeFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    m_keyValueDao.remove(queryContext, key);
    m_eventProvider->notifyRecordRemoved(queryContext, key);

    m_syncEngine->transactionLog()
        .generateTransactionAndSaveToLog<command::RemoveKeyValuePair>(
            queryContext, m_clusterId, Key{key});
}

void DataManager::insertOrUpdateReceivedRecord(
    nx::sql::QueryContext* queryContext,
    const std::string& /*clusterId*/,
    clusterdb::engine::Command<KeyValuePair> command)
{
    m_keyValueDao.insertOrUpdate(queryContext, command.params.key, command.params.value);
    m_eventProvider->notifyRecordInserted(queryContext, command.params.key, command.params.value);
}

void DataManager::removeReceivedRecord(
    nx::sql::QueryContext* queryContext,
    const std::string& /*clusterId*/,
    clusterdb::engine::Command<Key> command)
{
    m_keyValueDao.remove(queryContext, command.params.key);
    m_eventProvider->notifyRecordRemoved(queryContext, command.params.key);
}

} // namespace nx::clusterdb::map
