// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "db_connection_mock.h"

namespace nx::sql::test {

DbConnectionMock::DbConnectionMock(
    const ConnectionOptions& connectionOptions,
    std::shared_ptr<DBResult> forcedError)
    :
    QtDbConnection(connectionOptions),
    m_forcedError(forcedError)
{
}

DbConnectionMock::~DbConnectionMock()
{
    if (m_onDestructionHandler)
        m_onDestructionHandler();
}

bool DbConnectionMock::open()
{
    return *m_forcedError == DBResultCode::ok
        ? base_type::open()
        : false;
}

bool DbConnectionMock::begin()
{
    return *m_forcedError == DBResultCode::ok
        ? base_type::begin()
        : false;
}

DBResult DbConnectionMock::lastError()
{
    return *m_forcedError == DBResultCode::ok
        ? base_type::lastError()
        : *m_forcedError;
}

void DbConnectionMock::setOnDestructionHandler(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onDestructionHandler = std::move(handler);
}

} // namespace nx::sql::test
