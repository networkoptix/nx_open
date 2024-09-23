// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "db_connection_factory.h"

#include "qt_db_connection.h"

namespace nx::sql {

DbConnectionFactory::DbConnectionFactory():
    base_type([this](auto&&... args) {
        return defaultFunction(std::forward<decltype(args)>(args)...);
    })
{
}

DbConnectionFactory& DbConnectionFactory::instance()
{
    static DbConnectionFactory instance;
    return instance;
}

std::unique_ptr<AbstractDbConnection> DbConnectionFactory::defaultFunction(
    const nx::sql::ConnectionOptions& dbConnectionOptions)
{
    return std::make_unique<QtDbConnection>(dbConnectionOptions);
}

} // namespace nx::sql
