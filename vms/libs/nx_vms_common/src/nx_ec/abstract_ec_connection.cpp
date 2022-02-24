// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_ec_connection.h"

#include <nx/utils/log/assert.h>

#include "detail/call_sync.h"

namespace ec2 {

ECConnectionNotificationManager* AbstractECConnection::notificationManager()
{
    NX_ASSERT(0);
    return nullptr;
}

ErrorCode AbstractECConnection::dumpDatabaseSync(nx::vms::api::DatabaseDumpData* outData)
{
    return detail::callSync(
        [&](auto handler)
        {
            dumpDatabase(std::move(handler));
        },
        outData);
}

Result AbstractECConnection::dumpDatabaseToFileSync(const QString& dumpFilePath)
{
    return detail::callSync(
        [&](auto handler)
        {
            dumpDatabaseToFile(dumpFilePath, std::move(handler));
        });
}

ErrorCode AbstractECConnection::restoreDatabaseSync(const nx::vms::api::DatabaseDumpData& data)
{
    return detail::callSync(
        [&](auto handler)
        {
            restoreDatabase(data, std::move(handler));
        });
}

} // namespace ec2
