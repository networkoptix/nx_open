#pragma once

#include <QtSql/QSqlDatabase>

#include "abstract_db_connection.h"

namespace nx::sql {

class QtDbConnection:
    public AbstractDbConnection
{
public:
    QtDbConnection(const ConnectionOptions& connectionOptions);
    ~QtDbConnection();

    virtual bool open() override;
    virtual void close() override;

    virtual bool begin() override;
    virtual bool commit() override;
    virtual bool rollback() override;

    virtual DBResult lastError() override;
    virtual std::string lastErrorText() override;

    virtual std::unique_ptr<AbstractSqlQuery> createQuery() override;

    virtual QSqlDatabase* qtSqlConnection() override;

private:
    QString m_connectionName;
    QSqlDatabase m_connection;
    bool m_isOpen = false;
};

} // namespace nx::sql
