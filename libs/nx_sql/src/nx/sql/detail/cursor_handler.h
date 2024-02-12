// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

#include "../query.h"
#include "query_executor.h"

namespace nx::sql::detail {

class AbstractCursorHandler
{
public:
    virtual ~AbstractCursorHandler() = default;

    virtual nx::Uuid id() const = 0;
    virtual void initialize(AbstractDbConnection* const connection) = 0;
    virtual void reportErrorWithoutExecution(DBResult errorCode) = 0;
};

template<typename Record>
class CursorHandler:
    public AbstractCursorHandler
{
public:
    using PrepareCursorFunc = nx::utils::MoveOnlyFunc<void(SqlQuery*)>;
    using ReadRecordFunc = nx::utils::MoveOnlyFunc<void(SqlQuery*, Record*)>;
    using CursorCreatedHandler = nx::utils::MoveOnlyFunc<void(DBResult, nx::Uuid /*cursorId*/)>;

    CursorHandler(
        PrepareCursorFunc prepareCursorFunc,
        ReadRecordFunc readRecordFunc,
        CursorCreatedHandler cursorCreatedHandler)
        :
        m_id(nx::Uuid::createUuid()),
        m_prepareCursorFunc(std::move(prepareCursorFunc)),
        m_readRecordFunc(std::move(readRecordFunc)),
        m_cursorCreatedHandler(std::move(cursorCreatedHandler))
    {
    }

    virtual nx::Uuid id() const override
    {
        return m_id;
    }

    virtual void initialize(AbstractDbConnection* const connection) override
    {
        m_query = std::make_unique<SqlQuery>(connection);
        m_query->setForwardOnly(true);
        try
        {
            m_prepareCursorFunc(m_query.get());
            m_query->exec();
            nx::utils::swapAndCall(m_cursorCreatedHandler, DBResultCode::ok, m_id);
        }
        catch (const Exception& e)
        {
            nx::utils::swapAndCall(m_cursorCreatedHandler, e.dbResult(), nx::Uuid());
            throw;
        }
    }

    Record fetchNextRecord()
    {
        if (!m_query->next())
            throw Exception(DBResultCode::endOfData);
        Record record;
        m_readRecordFunc(m_query.get(), &record);
        return record;
    }

    virtual void reportErrorWithoutExecution(DBResult errorCode) override
    {
        if (m_cursorCreatedHandler)
            nx::utils::swapAndCall(m_cursorCreatedHandler, errorCode, nx::Uuid());
    }

private:
    nx::Uuid m_id;
    PrepareCursorFunc m_prepareCursorFunc;
    ReadRecordFunc m_readRecordFunc;
    CursorCreatedHandler m_cursorCreatedHandler;
    std::unique_ptr<SqlQuery> m_query;
};

//-------------------------------------------------------------------------------------------------

class NX_SQL_API CursorHandlerPool
{
public:
    void add(nx::Uuid id, std::unique_ptr<AbstractCursorHandler> cursorHandler);
    AbstractCursorHandler* cursorHander(nx::Uuid id);
    int cursorCount() const;
    void remove(nx::Uuid id);
    void cleanupDroppedCursors();
    void markCursorForDeletion(nx::Uuid id);

private:
    mutable nx::Mutex m_mutex;
    std::map<nx::Uuid, std::unique_ptr<AbstractCursorHandler>> m_cursors;
    std::vector<nx::Uuid> m_cursorsMarkedForDeletion;
};

//-------------------------------------------------------------------------------------------------

class NX_SQL_API BasicCursorOperationExecutor:
    public BaseExecutor
{
    using base_type = BaseExecutor;

public:
    BasicCursorOperationExecutor(CursorHandlerPool* cursorContextPool);

protected:
    CursorHandlerPool* cursorContextPool();

    virtual void executeCursor(AbstractDbConnection* const connection) = 0;

private:
    CursorHandlerPool* m_cursorContextPool;

    virtual DBResult executeQuery(AbstractDbConnection* const connection) override;
};

//-------------------------------------------------------------------------------------------------

class NX_SQL_API CursorCreator:
    public BasicCursorOperationExecutor
{
    using base_type = BasicCursorOperationExecutor;

public:
    CursorCreator(
        CursorHandlerPool* cursorContextPool,
        std::unique_ptr<AbstractCursorHandler> cursorHandler);

    virtual void reportErrorWithoutExecution(DBResult errorCode) override;
    virtual void setExternalTransaction(Transaction* transaction) override;

protected:
    virtual void executeCursor(AbstractDbConnection* const connection) override;

private:
    std::unique_ptr<AbstractCursorHandler> m_cursorHandler;
};

//-------------------------------------------------------------------------------------------------

class AbstractFetchNextRecordFromCursorTask
{
public:
    virtual ~AbstractFetchNextRecordFromCursorTask() = default;

    virtual nx::Uuid cursorId() const = 0;
    virtual void fetchNextRecordFrom(AbstractCursorHandler* cursorHandler) = 0;
    virtual void reportError(DBResult resultCode) = 0;
    virtual void reportRecord() = 0;
};

template<typename Record>
class FetchNextRecordFromCursorTask:
    public AbstractFetchNextRecordFromCursorTask
{
public:
    FetchNextRecordFromCursorTask(
        nx::Uuid cursorId,
        nx::utils::MoveOnlyFunc<void(DBResult, Record)> completionHandler)
        :
        m_cursorId(std::move(cursorId)),
        m_completionHandler(std::move(completionHandler))
    {
    }

    virtual nx::Uuid cursorId() const override
    {
        return m_cursorId;
    }

    virtual void fetchNextRecordFrom(AbstractCursorHandler* cursorHandler) override
    {
        auto typedCursorHandler = static_cast<CursorHandler<Record>*>(cursorHandler);
        m_record = typedCursorHandler->fetchNextRecord();
    }

    virtual void reportRecord() override
    {
        auto record = std::move(*m_record);
        m_record = std::nullopt;
        m_completionHandler(DBResultCode::ok, std::move(record));
    }

    virtual void reportError(DBResult resultCode) override
    {
        m_completionHandler(resultCode, Record());
    }

private:
    const nx::Uuid m_cursorId;
    nx::utils::MoveOnlyFunc<void(DBResult, Record)> m_completionHandler;
    std::optional<Record> m_record;
};

class FetchCursorDataExecutor:
    public BasicCursorOperationExecutor
{
    using base_type = BasicCursorOperationExecutor;

public:
    FetchCursorDataExecutor(
        CursorHandlerPool* cursorContextPool,
        std::unique_ptr<AbstractFetchNextRecordFromCursorTask> task)
        :
        base_type(cursorContextPool),
        m_task(std::move(task))
    {
    }

    virtual void reportErrorWithoutExecution(DBResult errorCode) override
    {
        cursorContextPool()->remove(m_task->cursorId());
        m_task->reportError(errorCode);
    }

    virtual void setExternalTransaction(Transaction* /*transaction*/) override
    {
    }

protected:
    virtual void executeCursor(AbstractDbConnection* const /*connection*/) override
    {
        auto cursorHandler = cursorContextPool()->cursorHander(m_task->cursorId());
        if (!cursorHandler)
        {
            m_task->reportError(DBResultCode::notFound);
            // Unknown cursor id is not a reason to close connection, so reporting ok.
            return;
        }

        try
        {
            m_task->fetchNextRecordFrom(cursorHandler);
            m_task->reportRecord();
        }
        catch (const Exception& e)
        {
            cursorContextPool()->remove(m_task->cursorId());
            m_task->reportError(e.dbResult());
            if (e.dbResult() != DBResultCode::endOfData) //< End of cursor data is not an error actually.
                throw;
        }
    }

private:
    std::unique_ptr<AbstractFetchNextRecordFromCursorTask> m_task;
};

//-------------------------------------------------------------------------------------------------

class CleanUpDroppedCursorsExecutor:
    public BasicCursorOperationExecutor
{
    using base_type = BasicCursorOperationExecutor;

public:
    CleanUpDroppedCursorsExecutor(CursorHandlerPool* cursorContextPool);

    virtual void reportErrorWithoutExecution(DBResult errorCode) override;
    virtual void setExternalTransaction(Transaction* transaction) override;

protected:
    virtual void executeCursor(AbstractDbConnection* const connection) override;
};

} // namespace nx::sql::detail
