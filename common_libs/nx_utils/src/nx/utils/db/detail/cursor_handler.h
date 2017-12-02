#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/uuid.h>

#include "../request_executor.h"
#include "../query.h"

namespace nx {
namespace utils {
namespace db {
namespace detail {

class AbstractCursorHandler
{
public:
    virtual ~AbstractCursorHandler() = default;

    virtual QnUuid id() const = 0;
    virtual void initialize(QSqlDatabase* const connection) = 0;
    virtual void reportErrorWithoutExecution(DBResult errorCode) = 0;
};

template<typename Record>
class CursorHandler:
    public AbstractCursorHandler
{
public:
    using PrepareCursorFunc = MoveOnlyFunc<void(SqlQuery*)>;
    using ReadRecordFunc = MoveOnlyFunc<void(SqlQuery*, Record*)>;
    using CursorCreatedHandler = MoveOnlyFunc<void(DBResult, QnUuid /*cursorId*/)>;

    CursorHandler(
        PrepareCursorFunc prepareCursorFunc,
        ReadRecordFunc readRecordFunc,
        CursorCreatedHandler cursorCreatedHandler)
        :
        m_id(QnUuid::createUuid()),
        m_prepareCursorFunc(std::move(prepareCursorFunc)),
        m_readRecordFunc(std::move(readRecordFunc)),
        m_cursorCreatedHandler(std::move(cursorCreatedHandler))
    {
    }

    virtual QnUuid id() const override
    {
        return m_id;
    }

    virtual void initialize(QSqlDatabase* const connection) override
    {
        m_query = std::make_unique<SqlQuery>(*connection);
        m_query->setForwardOnly(true);
        try
        {
            m_prepareCursorFunc(m_query.get());
            m_query->exec();
            nx::utils::swapAndCall(m_cursorCreatedHandler, DBResult::ok, m_id);
        }
        catch (Exception e)
        {
            nx::utils::swapAndCall(m_cursorCreatedHandler, e.dbResult(), QnUuid());
            throw;
        }
    }

    Record fetchNextRecord(QSqlDatabase* const /*connection*/)
    {
        if (!m_query->next())
            throw Exception(DBResult::endOfData);
        Record record;
        m_readRecordFunc(m_query.get(), &record);
        return record;
    }

    virtual void reportErrorWithoutExecution(DBResult errorCode) override
    {
        if (m_cursorCreatedHandler)
            nx::utils::swapAndCall(m_cursorCreatedHandler, errorCode, QnUuid());
    }

private:
    QnUuid m_id;
    PrepareCursorFunc m_prepareCursorFunc;
    ReadRecordFunc m_readRecordFunc;
    CursorCreatedHandler m_cursorCreatedHandler;
    std::unique_ptr<SqlQuery> m_query;
};

//-------------------------------------------------------------------------------------------------

class CursorHandlerPool
{
public:
    std::map<QnUuid, std::unique_ptr<AbstractCursorHandler>> cursors;
};

//-------------------------------------------------------------------------------------------------

class NX_UTILS_API CursorCreator:
    public BaseExecutor
{
    using base_type = BaseExecutor;

public:
    CursorCreator(
        CursorHandlerPool* cursorContextPool,
        std::unique_ptr<AbstractCursorHandler> cursorHandler);

    virtual void reportErrorWithoutExecution(DBResult errorCode) override;

protected:
    virtual DBResult executeQuery(QSqlDatabase* const connection) override;

private:
    CursorHandlerPool* m_cursorContextPool;
    std::unique_ptr<AbstractCursorHandler> m_cursorHandler;
};

//-------------------------------------------------------------------------------------------------

template<typename Record>
class FetchCursorDataExecutor:
    public BaseExecutor
{
public:
    FetchCursorDataExecutor(
        CursorHandlerPool* cursorContextPool,
        QnUuid cursorId,
        MoveOnlyFunc<void(DBResult, Record)> completionHandler)
        :
        BaseExecutor(QueryType::lookup),
        m_cursorContextPool(cursorContextPool),
        m_cursorId(cursorId),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual void reportErrorWithoutExecution(DBResult errorCode) override
    {
        m_cursorContextPool->cursors.erase(m_cursorId);
        m_completionHandler(errorCode, Record());
    }

protected:
    virtual DBResult executeQuery(QSqlDatabase* const connection) override
    {
        auto it = m_cursorContextPool->cursors.find(m_cursorId);
        if (it == m_cursorContextPool->cursors.end())
        {
            m_completionHandler(DBResult::notFound, Record());
            // Unknown cursor id is not a reason to close connection, so reporting ok.
            return DBResult::ok;
        }

        auto cursorHandler = static_cast<CursorHandler<Record>*>(it->second.get());
        try
        {
            auto record = cursorHandler->fetchNextRecord(connection);
            m_completionHandler(DBResult::ok, std::move(record));
            return DBResult::ok;
        }
        catch (Exception e)
        {
            m_cursorContextPool->cursors.erase(it);
            m_completionHandler(e.dbResult(), Record());
            if (e.dbResult() == DBResult::endOfData)
                return DBResult::ok; //< End of cursor data is not an error actually.
            throw;
        }
    }

private:
    CursorHandlerPool* m_cursorContextPool;
    QnUuid m_cursorId;
    MoveOnlyFunc<void(DBResult, Record)> m_completionHandler;
};

} // namespace detail
} // namespace db
} // namespace utils
} // namespace nx
