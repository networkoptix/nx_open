/**********************************************************
* Aug 11, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_REQUEST_EXECUTOR_H
#define NX_CLOUD_DB_REQUEST_EXECUTOR_H

#include <functional>

#include <QtSql/QSqlDatabase>

#include <nx/utils/move_only_func.h>

#include "types.h"


namespace nx {
namespace db {

class AbstractExecutor
{
public:
    virtual ~AbstractExecutor() {}

    //!Executed within DB thread
    virtual DBResult execute(QSqlDatabase* const connection) = 0;
};

class BaseExecutor
:
    public AbstractExecutor
{
protected:
    /** Returns more detailed result code if appropriate. Otherwise returns initial one */
    DBResult detailResultCode(QSqlDatabase* const connection, DBResult result) const;
    DBResult lastDBError(QSqlDatabase* const connection) const;
};

//!Executor of data update requests without output data
template<typename InputData>
class UpdateExecutor
:
    public BaseExecutor
{
public:
    UpdateExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase* const, const InputData&)> dbUpdateFunc,
        InputData&& input,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult, InputData)> completionHandler)
    :
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_input(std::move(input)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult execute(QSqlDatabase* const connection) override
    {
        auto completionHandler = std::move(m_completionHandler);
        if (!connection->transaction())
        {
            completionHandler(connection, lastDBError(connection), std::move(m_input));
            return DBResult::ioError;
        }
        auto result = m_dbUpdateFunc(connection, m_input);
        if (result != DBResult::ok)
        {
            result = detailResultCode(connection, result);
            connection->rollback();
            completionHandler(connection, result, std::move(m_input));
            return result;
        }

        if (!connection->commit())
        {
            const auto resultCode = lastDBError(connection);
            connection->rollback();
            completionHandler(connection, resultCode, std::move(m_input));
            return resultCode;
        }

        completionHandler(connection, DBResult::ok, std::move(m_input));
        return DBResult::ok;
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase* const, const InputData&)> m_dbUpdateFunc;
    InputData m_input;
    nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult, InputData)> m_completionHandler;
};

//!Executor of data update requests with output data
template<typename InputData, typename OutputData>
class UpdateWithOutputExecutor
:
    public BaseExecutor
{
public:
    UpdateWithOutputExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase* const, const InputData&, OutputData* const)> dbUpdateFunc,
        InputData&& input,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult, InputData, OutputData&&)> completionHandler)
    :
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_input(std::move(input)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult execute(QSqlDatabase* const connection) override
    {
        auto completionHandler = std::move(m_completionHandler);
        if (!connection->transaction())
        {
            completionHandler(connection, DBResult::ioError, std::move(m_input), OutputData());
            return DBResult::ioError;
        }
        OutputData output;
        auto result = m_dbUpdateFunc(connection, m_input, &output);
        if (result != DBResult::ok)
        {
            result = detailResultCode(connection, result);
            connection->rollback();
            completionHandler(connection, result, std::move(m_input), OutputData());
            return result;
        }

        if (!connection->commit())
        {
            const auto resultCode = lastDBError(connection);
            connection->rollback();
            completionHandler(connection, resultCode, std::move(m_input), OutputData());
            return resultCode;
        }

        completionHandler(
            connection,
            DBResult::ok,
            std::move(m_input),
            std::move(output));
        return DBResult::ok;
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase* const, const InputData&, OutputData* const)> m_dbUpdateFunc;
    InputData m_input;
    nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult, InputData, OutputData&&)> m_completionHandler;
};


class UpdateWithoutAnyDataExecutor
:
    public BaseExecutor
{
public:
    UpdateWithoutAnyDataExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase* const)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult)> completionHandler)
    :
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult execute(QSqlDatabase* const connection) override
    {
        auto completionHandler = std::move(m_completionHandler);
        if (!connection->transaction())
        {
            completionHandler(connection, DBResult::ioError);
            return DBResult::ioError;
        }
        auto result = m_dbUpdateFunc(connection);
        if (result != DBResult::ok)
        {
            result = detailResultCode(connection, result);
            connection->rollback();
            completionHandler(connection, result);
            return result;
        }

        if (!connection->commit())
        {
            const auto resultCode = lastDBError(connection);
            connection->rollback();
            completionHandler(connection, resultCode);
            return resultCode;
        }

        completionHandler(connection, DBResult::ok);
        return DBResult::ok;
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase* const)> m_dbUpdateFunc;
    nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult)> m_completionHandler;
};


class UpdateWithoutAnyDataExecutorNoTran
:
    public BaseExecutor
{
public:
    UpdateWithoutAnyDataExecutorNoTran(
        nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase* const)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult)> completionHandler)
    :
        m_dbUpdateFunc(std::move(dbUpdateFunc)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult execute(QSqlDatabase* const connection) override
    {
        auto completionHandler = std::move(m_completionHandler);
        auto result = m_dbUpdateFunc(connection);
        if (result != DBResult::ok)
            connection->rollback();
        result = detailResultCode(connection, result);
        completionHandler(connection, result);
        return result;
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase* const)> m_dbUpdateFunc;
    nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult)> m_completionHandler;
};


//!Executor of SELECT requests
template<typename OutputData>
class SelectExecutor
:
    public BaseExecutor
{
public:
    SelectExecutor(
        nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase*, OutputData* const)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult, OutputData)> completionHandler)
    :
        m_dbSelectFunc(std::move(dbSelectFunc)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual DBResult execute(QSqlDatabase* const connection) override
    {
        OutputData output;
        auto completionHandler = std::move(m_completionHandler);
        auto result = m_dbSelectFunc(connection, &output);
        result = detailResultCode(connection, result);
        completionHandler(connection, result, std::move(output));
        return result;
    }

private:
    nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase*, OutputData* const)> m_dbSelectFunc;
    nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult, OutputData)> m_completionHandler;
};

}   //namespace db
}   //namespace nx

#endif  //NX_CLOUD_DB_REQUEST_EXECUTOR_H
