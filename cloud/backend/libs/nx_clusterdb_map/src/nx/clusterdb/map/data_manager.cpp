#include "data_manager.h"

#include <nx/sql/query_context.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/clusterdb/engine/synchronization_engine.h>

#include "key_value_pair.h"
#include "dao/key_value_dao.h"

namespace nx::clusterdb::map {

namespace {

ResultCode toResultCode(nx::sql::DBResult dbResult)
{
    using namespace nx::sql;
    switch (dbResult)
    {
        case DBResult::ok:
            return ResultCode::ok;
        case DBResult::statementError:
            return ResultCode::statementError;
        case DBResult::ioError:
            return ResultCode::ioError;
        case DBResult::notFound:
            return ResultCode::notFound;
        case DBResult::cancelled:
            return ResultCode::cancelled;
        case DBResult::retryLater:
            return ResultCode::uniqueConstraintViolation;
        case DBResult::connectionError:
            return ResultCode::connectionError;
        case DBResult::logicError:
            return ResultCode::logicError;
        case DBResult::endOfData:
            return ResultCode::endOfData;
        default:
            return ResultCode::unknownError;
    }
}

} // namespace

DataManager::DataManager(
    nx::clusterdb::engine::SyncronizationEngine* syncronizationEngine,
    dao::KeyValueDao* keyValueDao,
    const std::string& systemId)
    :
    m_syncEngine(syncronizationEngine),
    m_keyValueDao(keyValueDao),
    m_systemId(systemId)
{
    using namespace std::placeholders;

    m_syncEngine->incomingCommandDispatcher()
        .registerCommandHandler<command::SaveKeyValuePair>(
            [this](auto&&... args)
            {
                return incomingSave(std::move(args)...);
            });

    m_syncEngine->incomingCommandDispatcher()
        .registerCommandHandler<command::RemoveKeyValuePair>(
            [this](auto&&... args)
            {
                return incomingRemove(std::move(args)...);
            });
}

DataManager::~DataManager()
{
    m_syncEngine->incomingCommandDispatcher()
        .removeHandler<command::SaveKeyValuePair>();

    m_syncEngine->incomingCommandDispatcher()
        .removeHandler<command::RemoveKeyValuePair>();
}

void DataManager::save(
    const std::string& key,
    const std::string& value,
    UpdateCompletionHander completionHandler)
{
    using namespace std::placeholders;

    m_syncEngine->transactionLog().startDbTransaction(
        m_systemId,
        [this, key, value](auto&&... args)
        {
            return saveToDb(std::move(args)..., key, value);
        },
        [this, completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult)
        {
            completionHandler(toResultCode(dbResult));
        });
}

void DataManager::remove(
    const std::string& key,
    UpdateCompletionHander completionHandler)
{
    using namespace std::placeholders;

    m_syncEngine->transactionLog().startDbTransaction(
        m_systemId,
        [this, key](auto&&... args)
        {
            return removeFromDb(std::move(args)..., key);
        },
        [completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult)
        {
            completionHandler(toResultCode(dbResult));
        });
}

void DataManager::get(
    const std::string& key,
    LookupCompletionHander completionHandler)
{
    auto value = std::make_shared<std::string>();

    m_keyValueDao->queryExecutor().executeSelect(
        [this, key, value](nx::sql::QueryContext* queryContext)
        {
            auto fetchResult = getFromDb(queryContext, key);
            if (fetchResult.second.has_value())
                *value = *fetchResult.second;
            return fetchResult.first;
        },
        [this, value, completionHandler = std::move(completionHandler)](
            nx::sql::DBResult dbResult)
        {
            completionHandler(toResultCode(dbResult), *value);
        }
    );
}

nx::sql::DBResult DataManager::saveToDb(
    nx::sql::QueryContext* queryContext,
    const std::string& key,
    const std::string& value)
{
    if (key.empty())
        return nx::sql::DBResult::logicError;

    m_keyValueDao->save(queryContext, key, value);

    return m_syncEngine->transactionLog()
        .generateTransactionAndSaveToLog<command::SaveKeyValuePair>(
            queryContext, m_systemId, KeyValuePair{key, value});
}

DataManager::FetchResult DataManager::getFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    if (key.empty())
        return FetchResult(nx::sql::DBResult::logicError, std::nullopt);

    return FetchResult(
        nx::sql::DBResult::ok,
        m_keyValueDao->get(queryContext, key));
}

nx::sql::DBResult DataManager::removeFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& key)
{
    if (key.empty())
        return nx::sql::DBResult::logicError;

    m_keyValueDao->remove(queryContext, key);

    return m_syncEngine->transactionLog()
        .generateTransactionAndSaveToLog<command::RemoveKeyValuePair>(
            queryContext, m_systemId, Key{key});
}

nx::sql::DBResult DataManager::incomingSave(
    nx::sql::QueryContext* queryContext,
    const std::string& /*systemId*/,
    clusterdb::engine::Command<KeyValuePair> command)
{
    m_keyValueDao->save(queryContext, command.params.key, command.params.value);
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult DataManager::incomingRemove(
    nx::sql::QueryContext* queryContext,
    const std::string& /*systemId*/,
    clusterdb::engine::Command<Key> command)
{
    m_keyValueDao->remove(queryContext, command.params.key);
    return nx::sql::DBResult::ok;
}

} // namespace nx::clusterdb::map