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

} // namespace

DataManager::DataManager(
    nx::clusterdb::engine::SyncronizationEngine* syncronizationEngine,
    nx::sql::AsyncSqlQueryExecutor* queryExecutor,
    const std::string& systemId,
    EventProvider* eventProvider)
    :
    m_syncEngine(syncronizationEngine),
    m_queryExecutor(queryExecutor),
    m_systemId(systemId),
    m_eventProvider(eventProvider),
    m_keyValueDao(systemId)
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
        m_systemId,
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
        m_systemId,
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

    auto value = std::make_shared<std::optional<std::string>>();

    m_queryExecutor->executeSelect(
        [this, key, value](nx::sql::QueryContext* queryContext) mutable
        {
            *value = getFromDb(queryContext, key);

            // Sql db doesn't throw notFound error if key lookup fails, so return it manually.
            return value->has_value()
                ? nx::sql::DBResult::ok
                : nx::sql::DBResult::notFound;
        },
        [value, completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult) mutable
        {
            completionHandler(
                toResultCode(dbResult),
                value->has_value() ? std::move(*(*value)) : std::string());
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
            return sharedKey->has_value()
                ? nx::sql::DBResult::ok
                : nx::sql::DBResult::notFound;
        },
        [this, sharedKey, completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult)
        {
            completionHandler(
                toResultCode(dbResult),
                sharedKey->has_value() ? std::move(*(*sharedKey)) : std::string());
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
            return sharedKey->has_value()
                ? nx::sql::DBResult::ok
                : nx::sql::DBResult::notFound;
        },
        [this, sharedKey, completionHandler = std::move(completionHandler)](
                nx::sql::DBResult dbResult)
        {
            completionHandler(
                toResultCode(dbResult),
                sharedKey->has_value() ? std::move(*(*sharedKey)) : std::string());
        });
}

void DataManager::getRange(
    const std::string& keyLowerBound,
    const std::string& keyUpperBound,
    GetRangeCompletionHandler completionHandler)
{
    if (keyLowerBound > keyUpperBound)
        return completionHandler(ResultCode::logicError, std::map<std::string, std::string>());

    auto sharedPairs = std::make_shared<std::map<std::string, std::string>>();

    m_queryExecutor->executeSelect(
        [this, keyLowerBound, keyUpperBound, sharedPairs](
            nx::sql::QueryContext* queryContext) mutable
        {
            *sharedPairs = getRangeFromDb(queryContext, keyLowerBound, keyUpperBound);
            return sharedPairs->empty()
                ? nx::sql::DBResult::notFound
                : nx::sql::DBResult::ok;
        },
        [this, sharedPairs, completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult)
        {
            completionHandler(toResultCode(dbResult), std::move(*sharedPairs));
        });
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
            queryContext, m_systemId, KeyValuePair{key, value});
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

std::map<std::string, std::string> DataManager::getRangeFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& keyLowerBound,
    const std::string& keyUpperBound)
{
    return m_keyValueDao.getRange(queryContext, keyLowerBound, keyUpperBound);
}

void DataManager::removeFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    m_keyValueDao.remove(queryContext, key);
    m_eventProvider->notifyRecordRemoved(queryContext, key);

    m_syncEngine->transactionLog()
        .generateTransactionAndSaveToLog<command::RemoveKeyValuePair>(
            queryContext, m_systemId, Key{key});
}

void DataManager::insertOrUpdateReceivedRecord(
    nx::sql::QueryContext* queryContext,
    const std::string& /*systemId*/,
    clusterdb::engine::Command<KeyValuePair> command)
{
    m_keyValueDao.insertOrUpdate(queryContext, command.params.key, command.params.value);
    m_eventProvider->notifyRecordInserted(queryContext, command.params.key, command.params.value);
}

void DataManager::removeReceivedRecord(
    nx::sql::QueryContext* queryContext,
    const std::string& /*systemId*/,
    clusterdb::engine::Command<Key> command)
{
    m_keyValueDao.remove(queryContext, command.params.key);
    m_eventProvider->notifyRecordRemoved(queryContext, command.params.key);
}

} // namespace nx::clusterdb::map