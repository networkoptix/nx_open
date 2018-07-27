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
#include "../types.h"
#include "../query_context.h"

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
        DBResult dbResult = DBResult::ok;
        try
        {
            dbResult = func(args...);
        }
        catch (const Exception& e)
        {
            dbResult = e.dbResult();
        }
        catch (const std::exception& e)
        {
            // TODO: #ak Propagate exception further so that "execute query sync" function throws this exception.
            NX_DEBUG(this, lm("Caught exception. %1").arg(e.what()));
            dbResult = DBResult::logicError;
        }
        return dbResult;
    }

private:
    StatisticsCollector* m_statisticsCollector;
    std::chrono::steady_clock::time_point m_creationTime;
    QueryExecutionInfo m_queryStatistics;
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
        if (result != DBResult::ok)
        {
            invokeCompletionHandlerWithDefaultValues(result);
            return result;
        }

        result = executeQueryUnderTransaction(&queryContext);
        if (result != DBResult::ok)
        {
            transaction.rollback();
            return result;
        }

        result = transaction.commit();
        if (result != DBResult::ok)
        {
            transaction.rollback();
            return result;
        }

        return DBResult::ok;
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
        if (result != DBResult::ok)
        {
            queryContext->transaction()->addOnTransactionCompletionHandler(
                std::bind(&BaseUpdateExecutor::invokeCompletionHandlerWithDefaultValues, this,
                    result));
            return result;
        }

        queryContext->transaction()->addOnTransactionCompletionHandler(
            std::bind(&BaseUpdateExecutor::reportQueryResult, this,
                queryContext->connection(), std::placeholders::_1));

        return DBResult::ok;
    }

    DBResult executeQueryWithoutTransaction(QueryContext* queryContext)
    {
        auto result = doQuery(queryContext);
        if (result != DBResult::ok)
        {
            invokeCompletionHandlerWithDefaultValues(result);
            return result;
        }

        reportSuccess();

        return DBResult::ok;
    }

    void reportQueryResult(AbstractDbConnection* connection, DBResult dbResult)
    {
        if (dbResult == DBResult::ok)
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

/**
 * Executor of data update requests without output data.
 */
template<typename InputData>
class UpdateExecutor:
    public BaseUpdateExecutor<>
{
    using base_type = BaseUpdateExecutor<>;

public:
    UpdateExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const, const InputData&)> dbUpdateFunc,
        InputData&& input,
        nx::utils::MoveOnlyFunc<void(DBResult, InputData)> completionHandler,
        const std::string& queryAggregationKey)
    :
        base_type(
            std::bind(&UpdateExecutor::invokeCompletionHandler2, this, std::placeholders::_1),
            queryAggregationKey),
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_input(std::move(input)),
        m_completionHandler(std::move(completionHandler))
    {
    }

protected:
    virtual DBResult doQuery(QueryContext* queryContext) override
    {
        return this->invokeDbQueryFunc(m_dbUpdateFunc, queryContext, m_input);
    }

    virtual void reportSuccess() override
    {
        this->invokeCompletionHandler(DBResult::ok);
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const, const InputData&)> m_dbUpdateFunc;
    InputData m_input;
    nx::utils::MoveOnlyFunc<void(DBResult, InputData)> m_completionHandler;

    void invokeCompletionHandler2(DBResult dbResult)
    {
        m_completionHandler(dbResult, std::move(m_input));
    }
};

//-------------------------------------------------------------------------------------------------
// UpdateWithOutputExecutor

/**
 * Executor of data update requests with output data.
 */
template<typename InputData, typename OutputData>
class UpdateWithOutputExecutor:
    public BaseUpdateExecutor<OutputData>
{
    using base_type = BaseUpdateExecutor<OutputData>;

public:
    UpdateWithOutputExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const, const InputData&, OutputData* const)> dbUpdateFunc,
        InputData&& input,
        nx::utils::MoveOnlyFunc<void(DBResult, InputData, OutputData&&)> completionHandler,
        const std::string& queryAggregationKey)
    :
        base_type(
            std::bind(&UpdateWithOutputExecutor::invokeCompletionHandler2, this,
                std::placeholders::_1, std::placeholders::_2),
            queryAggregationKey),
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_input(std::move(input)),
        m_completionHandler(std::move(completionHandler))
    {
    }

protected:
    virtual DBResult doQuery(QueryContext* queryContext) override
    {
        return this->invokeDbQueryFunc(m_dbUpdateFunc, queryContext, m_input, &m_output);
    }

    virtual void reportSuccess() override
    {
        this->invokeCompletionHandler(DBResult::ok, std::move(m_output));
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const, const InputData&, OutputData* const)> m_dbUpdateFunc;
    InputData m_input;
    nx::utils::MoveOnlyFunc<void(DBResult, InputData, OutputData&&)> m_completionHandler;
    OutputData m_output;

    void invokeCompletionHandler2(
        DBResult dbResult, OutputData outputData)
    {
        m_completionHandler(dbResult, std::move(m_input), std::move(outputData));
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
// UpdateWithoutAnyDataExecutorNoTran

class NX_SQL_API UpdateWithoutAnyDataExecutorNoTran:
    public BaseUpdateExecutor<>
{
    using base_type = BaseUpdateExecutor<>;

public:
    UpdateWithoutAnyDataExecutorNoTran(
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
