#pragma once

#include <functional>

#include <QtSql/QSqlDatabase>

#include <nx/utils/move_only_func.h>

#include "types.h"
#include "query_context.h"

namespace nx {
namespace db {

class AbstractExecutor
{
public:
    virtual ~AbstractExecutor() {}

    /** Executed within DB connection thread. */
    virtual DBResult execute(QSqlDatabase* const connection) = 0;
    virtual void reportErrorWithoutExecution(DBResult errorCode) = 0;
};

class BaseExecutor:
    public AbstractExecutor
{
protected:
    /** Returns more detailed result code if appropriate. Otherwise returns initial one. */
    DBResult detailResultCode(QSqlDatabase* const connection, DBResult result) const;
    DBResult lastDBError(QSqlDatabase* const connection) const;
};

/**
 * Executor of data update requests without output data.
 */
template<typename InputData>
class UpdateExecutor:
    public BaseExecutor
{
public:
    UpdateExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const, const InputData&)> dbUpdateFunc,
        InputData&& input,
        nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult, InputData)> completionHandler)
    :
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_input(std::move(input)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult execute(QSqlDatabase* const connection) override
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

        result = m_dbUpdateFunc(&queryContext, m_input);
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

/**
 * Executor of data update requests with output data.
 */
template<typename InputData, typename OutputData>
class UpdateWithOutputExecutor:
    public BaseExecutor
{
public:
    UpdateWithOutputExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const, const InputData&, OutputData* const)> dbUpdateFunc,
        InputData&& input,
        nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult, InputData, OutputData&&)> completionHandler)
    :
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_input(std::move(input)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult execute(QSqlDatabase* const connection) override
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
        result = m_dbUpdateFunc(&queryContext, m_input, &output);
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

class UpdateWithoutAnyDataExecutor:
    public BaseExecutor
{
public:
    UpdateWithoutAnyDataExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> completionHandler)
    :
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult execute(QSqlDatabase* const connection) override
    {
        Transaction transaction(connection);
        QueryContext queryContext(connection, &transaction);

        auto completionHandler = std::move(m_completionHandler);
        auto result = transaction.begin();
        if (result != DBResult::ok)
        {
            result = lastDBError(connection);
            completionHandler(&queryContext, result);
            return result;
        }

        result = m_dbUpdateFunc(&queryContext);
        if (result != DBResult::ok)
        {
            result = detailResultCode(connection, result);
            transaction.rollback();
            completionHandler(&queryContext, result);
            return result;
        }

        result = transaction.commit();
        if (result != DBResult::ok)
        {
            result = lastDBError(connection);
            connection->rollback();
            completionHandler(&queryContext, result);
            return result;
        }

        completionHandler(&queryContext, DBResult::ok);
        return DBResult::ok;
    }

    virtual void reportErrorWithoutExecution(DBResult errorCode) override
    {
        m_completionHandler(nullptr, errorCode);
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> m_dbUpdateFunc;
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> m_completionHandler;
};

class UpdateWithoutAnyDataExecutorNoTran:
    public BaseExecutor
{
public:
    UpdateWithoutAnyDataExecutorNoTran(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> completionHandler)
    :
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult execute(QSqlDatabase* const connection) override
    {
        auto completionHandler = std::move(m_completionHandler);
        QueryContext queryContext(connection, nullptr);
        auto result = m_dbUpdateFunc(&queryContext);
        result = detailResultCode(connection, result);
        completionHandler(&queryContext, result);
        return result;
    }

    virtual void reportErrorWithoutExecution(DBResult errorCode) override
    {
        m_completionHandler(nullptr, errorCode);
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> m_dbUpdateFunc;
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> m_completionHandler;
};

//!Executor of SELECT requests
class SelectExecutor:
    public BaseExecutor
{
public:
    SelectExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> completionHandler)
    :
        m_dbSelectFunc(std::move(dbSelectFunc)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult execute(QSqlDatabase* const connection) override
    {
        auto completionHandler = std::move(m_completionHandler);
        QueryContext queryContext(connection, nullptr);
        auto result = m_dbSelectFunc(&queryContext);
        result = detailResultCode(connection, result);
        completionHandler(&queryContext, result);
        return result;
    }

    virtual void reportErrorWithoutExecution(DBResult errorCode) override
    {
        m_completionHandler(nullptr, errorCode);
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QueryContext*)> m_dbSelectFunc;
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> m_completionHandler;
};

} // namespace db
} // namespace nx
