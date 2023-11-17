// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtSql/QSqlDatabase>

#include "abstract_db_connection.h"

namespace nx::sql {

class NX_SQL_API QtDbConnection:
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

    virtual std::unique_ptr<AbstractSqlQuery> createQuery() override;

    virtual RdbmsDriverType driverType() const override;

    virtual QSqlDatabase* qtSqlConnection() override;

    virtual bool tableExist(const std::string_view& tableName) override;

private:
    QString m_connectionName;
    QSqlDatabase m_connection;
    bool m_isOpen = false;
    RdbmsDriverType m_driverType;
};

} // namespace nx::sql
