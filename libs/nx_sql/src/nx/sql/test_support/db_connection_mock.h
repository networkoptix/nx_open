// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/move_only_func.h>

#include "../qt_db_connection.h"

namespace nx::sql::test {

class NX_SQL_API DbConnectionMock:
    public QtDbConnection
{
    using base_type = QtDbConnection;

public:
    DbConnectionMock(
        const ConnectionOptions& connectionOptions,
        std::shared_ptr<DBResult> forcedError);

    ~DbConnectionMock();

    virtual bool open() override;
    virtual bool begin() override;
    virtual DBResult lastError() override;
    void setOnDestructionHandler(nx::utils::MoveOnlyFunc<void()> handler);

private:
    nx::utils::MoveOnlyFunc<void()> m_onDestructionHandler;
    std::shared_ptr<DBResult> m_forcedError;
};

} // namespace nx::sql::test
