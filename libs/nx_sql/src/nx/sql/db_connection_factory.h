// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/basic_factory.h>

#include "abstract_db_connection.h"

namespace nx::sql {

using DbConnectionFactoryFunction =
    std::unique_ptr<AbstractDbConnection>(const nx::sql::ConnectionOptions& dbConnectionOptions);

class NX_SQL_API DbConnectionFactory:
    public nx::utils::BasicFactory<DbConnectionFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<DbConnectionFactoryFunction>;

public:
    DbConnectionFactory();

    static DbConnectionFactory& instance();

private:
    std::unique_ptr<AbstractDbConnection> defaultFunction(
        const nx::sql::ConnectionOptions& dbConnectionOptions);
};

} // namespace nx::sql
