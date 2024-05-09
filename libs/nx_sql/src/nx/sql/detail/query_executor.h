// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <tuple>

#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/optional.h>

#include "../abstract_db_connection.h"
#include "../db_connection_holder.h"
#include "../db_statistics_collector.h"
#include "../query_context.h"
#include "../types.h"

namespace nx::sql::detail {

class NX_SQL_API AbstractExecutor
{
public:
    virtual ~AbstractExecutor() = default;

    /** Executed within DB connection thread. */
    virtual DBResult execute(AbstractDbConnection* const connection) = 0;
    virtual void reportErrorWithoutExecution(DBResult errorCode) = 0;
    virtual QueryType queryType() const = 0;
    virtual void setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler) = 0;
    virtual void setExternalTransaction(Transaction* transaction) = 0;
    virtual std::string aggregationKey() const = 0;
};

//-------------------------------------------------------------------------------------------------
// BaseExecutor

class NX_SQL_API BaseExecutor:
    public AbstractExecutor
{
public:
    BaseExecutor(
        QueryType queryType,
        const std::string& queryAggregationKey);
    virtual ~BaseExecutor() override;

    virtual DBResult execute(AbstractDbConnection* const connection) override;
    virtual QueryType queryType() const override;
    virtual void setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler) override;
    virtual std::string aggregationKey() const override;

    void setStatisticsCollector(StatisticsCollector* statisticsCollector);

protected:
    virtual DBResult executeQuery(AbstractDbConnection* const connection) = 0;

    template<typename Func, typename... Args>
    DBResult invokeDbQueryFunc(Func& func, const Args&... args)
    {
        DBResult dbResult;
        try
        {
            dbResult = func(args...);
        }
        catch (const Exception& e)
        {
            NX_DEBUG(this, nx::format("Caught DB exception. %1").arg(e.what()));
            dbResult = e.dbResult();
        }
        catch (const std::exception& e)
        {
            // TODO: #akolesnikov Propagate exception further so that "execute query sync"
            // function throws this exception.
            NX_DEBUG(this, "Caught exception. %1", e.what());
            dbResult = DBResult(DBResultCode::logicError, e.what());
        }

        return dbResult;
    }

private:
    StatisticsCollector* m_statisticsCollector;
    std::chrono::steady_clock::time_point m_creationTime;
    QueryExecutionTaskRecord m_queryStatistics;
    bool m_queryExecuted;
    nx::utils::MoveOnlyFunc<void()> m_onBeforeDestructionHandler;
    QueryType m_queryType;
    const std::string m_queryAggregationKey;
};

//-------------------------------------------------------------------------------------------------
// UpdateExecutor

class AbstractUpdateExecutor:
    public BaseExecutor
{
public:
    AbstractUpdateExecutor(const std::string& queryAggregationKey):
        BaseExecutor(QueryType::modification, queryAggregationKey)
    {
    }
};

template<typename ... CompletionHandlerArgs>
class BaseUpdateExecutor:
    public AbstractUpdateExecutor
{
    using base_type = AbstractUpdateExecutor;

public:
    using CompletionHandler =
        nx::utils::MoveOnlyFunc<void(DBResult, CompletionHandlerArgs...)>;

    BaseUpdateExecutor(
        CompletionHandler completionHandler,
        const std::string& queryAggregationKey)
        :
        base_type(queryAggregationKey),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult executeQuery(AbstractDbConnection* const connection) override
    {
        if (m_externalTransaction)
        {
            if (*m_externalTransaction)
            {
                QueryContext queryContext(connection, *m_externalTransaction);
                return executeQueryUnderTransaction(&queryContext);
            }
            else
            {
                QueryContext queryContext(connection, nullptr);
                return executeQueryWithoutTransaction(&queryContext);
            }
        }

        return executeQueryUnderNewTransaction(connection);
    }

    virtual void reportErrorWithoutExecution(DBResult errorCode) override
    {
        invokeCompletionHandlerWithDefaultValues(errorCode);
    }

    virtual void setExternalTransaction(Transaction* transaction) override
    {
        m_externalTransaction = transaction;
    }

protected:
    virtual DBResult doQuery(QueryContext* queryContext) = 0;

    virtual void reportSuccess() = 0;

    void invokeCompletionHandler(
        DBResult resultCode,
        CompletionHandlerArgs... args)
    {
        CompletionHandler completionHandler;
        completionHandler.swap(m_completionHandler);

        std::apply(
            completionHandler,
            std::tuple_cat(
                std::make_tuple(resultCode),
                std::make_tuple(std::move(args)...)));
    }

private:
    CompletionHandler m_completionHandler;
    std::optional<Transaction*> m_externalTransaction;

    DBResult executeQueryUnderNewTransaction(AbstractDbConnection* const connection)
    {
        Transaction transaction(connection);
        QueryContext queryContext(connection, &transaction);

        auto result = transaction.begin();
        if (result != DBResultCode::ok)
        {
            invokeCompletionHandlerWithDefaultValues(result);
            return result;
        }

        result = executeQueryUnderTransaction(&queryContext);
        if (result != DBResultCode::ok)
        {
            transaction.rollback();
            return result;
        }

        result = transaction.commit();
        if (result != DBResultCode::ok)
        {
            transaction.rollback();
            return result;
        }

        return DBResultCode::ok;
    }

    void invokeCompletionHandlerWithDefaultValues(DBResult errorCode)
    {
        CompletionHandler completionHandler;
        completionHandler.swap(m_completionHandler);

        std::tuple<CompletionHandlerArgs...> defaultArgs;
        std::apply(
            completionHandler,
            std::tuple_cat(std::make_tuple(errorCode), std::move(defaultArgs)));
    }

    DBResult executeQueryUnderTransaction(QueryContext* queryContext)
    {
        auto result = doQuery(queryContext);
        if (result != DBResultCode::ok)
        {
            queryContext->transaction()->addOnTransactionCompletionHandler(
                std::bind(&BaseUpdateExecutor::invokeCompletionHandlerWithDefaultValues, this,
                    result));
            return result;
        }

        queryContext->transaction()->addOnTransactionCompletionHandler(
            std::bind(&BaseUpdateExecutor::reportQueryResult, this,
                queryContext->connection(), std::placeholders::_1));

        return DBResultCode::ok;
    }

    DBResult executeQueryWithoutTransaction(QueryContext* queryContext)
    {
        auto result = doQuery(queryContext);
        if (result != DBResultCode::ok)
        {
            invokeCompletionHandlerWithDefaultValues(result);
            return result;
        }

        reportSuccess();

        return DBResultCode::ok;
    }

    void reportQueryResult(AbstractDbConnection* connection, DBResult dbResult)
    {
        if (dbResult == DBResultCode::ok)
        {
            // In case of transaction success (query succeeded & committed).
            reportSuccess();
        }
        else
        {
            // In case of failed transaction (e.g., commit fails).
            invokeCompletionHandlerWithDefaultValues(connection->lastError());
        }
    }
};

//-------------------------------------------------------------------------------------------------
// UpdateWithoutAnyDataExecutor

class NX_SQL_API UpdateWithoutAnyDataExecutor:
    public BaseUpdateExecutor<>
{
    using base_type = BaseUpdateExecutor<>;

public:
    UpdateWithoutAnyDataExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
        const std::string& queryAggregationKey);

protected:
    virtual DBResult doQuery(QueryContext* queryContext) override;

    virtual void reportSuccess() override;

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> m_dbUpdateFunc;
};

//-------------------------------------------------------------------------------------------------
// SelectExecutor

class NX_SQL_API SelectExecutor:
    public BaseExecutor
{
    using base_type = BaseExecutor;

public:
    SelectExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
        const std::string& queryAggregationKey);

    virtual DBResult executeQuery(AbstractDbConnection* const connection) override;
    virtual void reportErrorWithoutExecution(DBResult errorCode) override;
    virtual void setExternalTransaction(Transaction* transaction) override;

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext*)> m_dbSelectFunc;
    nx::utils::MoveOnlyFunc<void(DBResult)> m_completionHandler;
};

} // namespace nx::sql::detail
