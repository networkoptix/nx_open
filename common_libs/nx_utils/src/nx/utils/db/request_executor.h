#pragma once

#include <chrono>
#include <functional>

#include <boost/optional.hpp>

#include <QtSql/QSqlDatabase>

#include <nx/utils/log/log.h>
#include <nx/utils/move_only_func.h>

#include "db_statistics_collector.h"
#include "types.h"
#include "query_context.h"

namespace nx {
namespace utils {
namespace db {

class StatisticsCollector;

enum class QueryType
{
    lookup,
    modification,
};

class NX_UTILS_API AbstractExecutor
{
public:
    virtual ~AbstractExecutor() = default;

    /** Executed within DB connection thread. */
    virtual DBResult execute(QSqlDatabase* const connection) = 0;
    virtual void reportErrorWithoutExecution(DBResult errorCode) = 0;
    virtual QueryType queryType() const = 0;
    virtual void setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler) = 0;
};

//-------------------------------------------------------------------------------------------------
// BaseExecutor

class NX_UTILS_API BaseExecutor:
    public AbstractExecutor
{
public:
    BaseExecutor(QueryType queryType);
    virtual ~BaseExecutor() override;

    virtual DBResult execute(QSqlDatabase* const connection) override;
    virtual QueryType queryType() const override;
    virtual void setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler) override;

    void setStatisticsCollector(StatisticsCollector* statisticsCollector);

protected:
    virtual DBResult executeQuery(QSqlDatabase* const connection) = 0;

    /**
     * Returns more detailed result code if appropriate. Otherwise returns initial one.
     */
    DBResult detailResultCode(QSqlDatabase* const connection, DBResult result) const;
    DBResult lastDBError(QSqlDatabase* const connection) const;

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
};

//-------------------------------------------------------------------------------------------------
// UpdateExecutor

/**
 * Executor of data update requests without output data.
 */
template<typename InputData>
class UpdateExecutor:
    public BaseExecutor
{
    using base_type = BaseExecutor;

public:
    UpdateExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const, const InputData&)> dbUpdateFunc,
        InputData&& input,
        nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult, InputData)> completionHandler)
    :
        base_type(QueryType::modification),
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_input(std::move(input)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult executeQuery(QSqlDatabase* const connection) override
    {
        Transaction transaction(connection);
        QueryContext queryContext(connection, &transaction);

        auto completionHandler = std::move(m_completionHandler);
        auto result = transaction.begin();
        if (result != DBResult::ok)
        {
            result = lastDBError(connection);
            completionHandler(&queryContext, result, std::move(m_input));
            return result;
        }

        result = invokeDbQueryFunc(m_dbUpdateFunc, &queryContext, m_input);
        if (result != DBResult::ok)
        {
            result = detailResultCode(connection, result);
            transaction.rollback();
            completionHandler(&queryContext, result, std::move(m_input));
            return result;
        }

        result = transaction.commit();
        if (result != DBResult::ok)
        {
            result = lastDBError(connection);
            connection->rollback();
            completionHandler(&queryContext, result, std::move(m_input));
            return result;
        }

        completionHandler(&queryContext, DBResult::ok, std::move(m_input));
        return DBResult::ok;
    }

    virtual void reportErrorWithoutExecution(DBResult errorCode) override
    {
        m_completionHandler(nullptr, errorCode, InputData());
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const, const InputData&)> m_dbUpdateFunc;
    InputData m_input;
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult, InputData)> m_completionHandler;
};

//-------------------------------------------------------------------------------------------------
// UpdateWithOutputExecutor

/**
 * Executor of data update requests with output data.
 */
template<typename InputData, typename OutputData>
class UpdateWithOutputExecutor:
    public BaseExecutor
{
    using base_type = BaseExecutor;

public:
    UpdateWithOutputExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const, const InputData&, OutputData* const)> dbUpdateFunc,
        InputData&& input,
        nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult, InputData, OutputData&&)> completionHandler)
    :
        base_type(QueryType::modification),
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_input(std::move(input)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult executeQuery(QSqlDatabase* const connection) override
    {
        Transaction transaction(connection);
        QueryContext queryContext(connection, &transaction);

        auto completionHandler = std::move(m_completionHandler);
        auto result = transaction.begin();
        if (result != DBResult::ok)
        {
            result = lastDBError(connection);
            completionHandler(&queryContext, result, std::move(m_input), OutputData());
            return result;
        }

        OutputData output;
        result = invokeDbQueryFunc(m_dbUpdateFunc, &queryContext, m_input, &output);
        if (result != DBResult::ok)
        {
            result = detailResultCode(connection, result);
            transaction.rollback();
            completionHandler(&queryContext, result, std::move(m_input), OutputData());
            return result;
        }

        result = transaction.commit();
        if (result != DBResult::ok)
        {
            result = lastDBError(connection);
            connection->rollback();
            completionHandler(&queryContext, result, std::move(m_input), OutputData());
            return result;
        }

        completionHandler(&queryContext, DBResult::ok, std::move(m_input), std::move(output));
        return DBResult::ok;
    }

    virtual void reportErrorWithoutExecution(DBResult errorCode) override
    {
        m_completionHandler(nullptr, errorCode, InputData(), OutputData());
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const, const InputData&, OutputData* const)> m_dbUpdateFunc;
    InputData m_input;
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult, InputData, OutputData&&)> m_completionHandler;
};

//-------------------------------------------------------------------------------------------------
// UpdateWithoutAnyDataExecutor

class NX_UTILS_API UpdateWithoutAnyDataExecutor:
    public BaseExecutor
{
    using base_type = BaseExecutor;

public:
    UpdateWithoutAnyDataExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> completionHandler);

    virtual DBResult executeQuery(QSqlDatabase* const connection) override;
    virtual void reportErrorWithoutExecution(DBResult errorCode) override;

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> m_dbUpdateFunc;
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> m_completionHandler;
};

//-------------------------------------------------------------------------------------------------
// UpdateWithoutAnyDataExecutorNoTran

class NX_UTILS_API UpdateWithoutAnyDataExecutorNoTran:
    public BaseExecutor
{
    using base_type = BaseExecutor;

public:
    UpdateWithoutAnyDataExecutorNoTran(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> completionHandler);

    virtual DBResult executeQuery(QSqlDatabase* const connection) override;
    virtual void reportErrorWithoutExecution(DBResult errorCode) override;

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> m_dbUpdateFunc;
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> m_completionHandler;
};

//-------------------------------------------------------------------------------------------------
// SelectExecutor

class NX_UTILS_API SelectExecutor:
    public BaseExecutor
{
    using base_type = BaseExecutor;

public:
    SelectExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> completionHandler);

    virtual DBResult executeQuery(QSqlDatabase* const connection) override;
    virtual void reportErrorWithoutExecution(DBResult errorCode) override;

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext*)> m_dbSelectFunc;
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> m_completionHandler;
};

} // namespace db
} // namespace utils
} // namespace nx
