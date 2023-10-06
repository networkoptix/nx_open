// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sql/abstract_db_connection.h>
#include <nx/sql/async_sql_query_executor.h>

namespace nx::sql::test {

class DelegateSqlQuery:
    public AbstractSqlQuery
{
public:
    DelegateSqlQuery(std::unique_ptr<AbstractSqlQuery> delegate):
        m_delegate(std::move(delegate))
    {
    }

    virtual void setForwardOnly(bool val) override
    {
        return m_delegate->setForwardOnly(val);
    }

    virtual void prepare(const std::string_view& query) override
    {
        return m_delegate->prepare(query);
    }

    virtual void addBindValue(const QVariant& value) noexcept override
    {
        return m_delegate->addBindValue(value);
    }

    virtual void addBindValue(const std::string_view& value) noexcept override
    {
        return m_delegate->addBindValue(value);
    }

    virtual void bindValue(const QString& placeholder, const QVariant& value) noexcept override
    {
        return m_delegate->bindValue(placeholder, value);
    }

    virtual void bindValue(int pos, const QVariant& value) noexcept override
    {
        return m_delegate->bindValue(pos, value);
    }

    virtual void bindValue(const std::string_view& placeholder, const std::string_view& value) noexcept override
    {
        return m_delegate->bindValue(placeholder, value);
    }

    virtual void bindValue(int pos, const std::string_view& value) noexcept override
    {
        return m_delegate->bindValue(pos, value);
    }

    virtual void exec() override
    {
        return m_delegate->exec();
    }

    virtual bool next() override
    {
        return m_delegate->next();
    }

    virtual QVariant value(int index) const override
    {
        return m_delegate->value(index);
    }

    virtual QVariant value(const QString& name) const override
    {
        return m_delegate->value(name);
    }

    virtual QSqlRecord record() override
    {
        return m_delegate->record();
    }

    virtual QVariant lastInsertId() const override
    {
        return m_delegate->lastInsertId();
    }

    virtual int numRowsAffected() const override
    {
        return m_delegate->numRowsAffected();
    }

    virtual QSqlQuery& impl() override
    {
        return m_delegate->impl();
    }

    virtual const QSqlQuery& impl() const override
    {
        return m_delegate->impl();
    }

private:
    std::unique_ptr<AbstractSqlQuery> m_delegate;
};

//-------------------------------------------------------------------------------------------------

class DelegateDbConnection:
    public AbstractDbConnection
{
public:
    DelegateDbConnection(AbstractDbConnection* delegate):
        m_delegate(delegate)
    {
    }

    virtual bool open()
    {
        return m_delegate->open();
    }

    virtual void close()
    {
        return m_delegate->close();
    }

    virtual bool begin()
    {
        return m_delegate->begin();
    }

    virtual bool commit()
    {
        return m_delegate->commit();
    }

    virtual bool rollback()
    {
        return m_delegate->rollback();
    }

    virtual DBResult lastError()
    {
        return m_delegate->lastError();
    }

    virtual std::unique_ptr<AbstractSqlQuery> createQuery()
    {
        return m_delegate->createQuery();
    }

    virtual RdbmsDriverType driverType() const
    {
        return m_delegate->driverType();
    }

    virtual QSqlDatabase* qtSqlConnection()
    {
        return m_delegate->qtSqlConnection();
    }

private:
    AbstractDbConnection* m_delegate = nullptr;
};

//-------------------------------------------------------------------------------------------------

class DelegateAsyncSqlQueryExecutor:
    public AbstractAsyncSqlQueryExecutor
{
public:
    DelegateAsyncSqlQueryExecutor(AbstractAsyncSqlQueryExecutor* _delegate):
        m_delegate(_delegate)
    {
    }

    virtual void pleaseStopSync() override
    {
        return m_delegate->pleaseStopSync();
    }

    virtual const ConnectionOptions& connectionOptions() const override
    {
        return m_delegate->connectionOptions();
    }

    virtual void setQueryPriority(QueryType queryType, int newPriority) override
    {
        m_delegate->setQueryPriority(queryType, newPriority);
    }

    virtual void setConcurrentModificationQueryLimit(int limit) override
    {
        return m_delegate->setConcurrentModificationQueryLimit(limit);
    }

    virtual void setQueryTimeoutEnabled(bool enabled) override
    {
        return m_delegate->setQueryTimeoutEnabled(enabled);
    }

    virtual QueryQueueStats queryQueueStatistics() const override
    {
        return m_delegate->queryQueueStatistics();
    }

    virtual QueryStatistics queryStatistics() const override
    {
        return m_delegate->queryStatistics();
    }

    //---------------------------------------------------------------------------------------------
    // Asynchronous operations.

    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
        const std::string& queryAggregationKey) override
    {
        return m_delegate->executeUpdate(
            std::move(dbUpdateFunc),
            std::move(completionHandler),
            queryAggregationKey);
    }

    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler) override
    {
        m_delegate->executeSelect(std::move(dbSelectFunc), std::move(completionHandler));
    }

    //---------------------------------------------------------------------------------------------
    // Synchronous operations.

    virtual DBResult execSqlScript(
        nx::sql::QueryContext* const queryContext,
        const std::string& script) override
    {
        return m_delegate->execSqlScript(queryContext, script);
    }

    //---------------------------------------------------------------------------------------------

    virtual void createCursorImpl(
        std::unique_ptr<detail::AbstractCursorHandler> cursorHandler) override
    {
        m_delegate->createCursorImpl(std::move(cursorHandler));
    }

    virtual void fetchNextRecordFromCursorImpl(
        std::unique_ptr<detail::AbstractFetchNextRecordFromCursorTask> task) override
    {
        m_delegate->fetchNextRecordFromCursorImpl(std::move(task));
    }

    virtual void removeCursor(QnUuid id) override
    {
        m_delegate->removeCursor(id);
    }

    virtual int openCursorCount() const override
    {
        return m_delegate->openCursorCount();
    }

private:
    AbstractAsyncSqlQueryExecutor* m_delegate = nullptr;
};

} // namespace nx::sql::test
