// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_factory_context.h"

#include "remote_connection.h"

namespace nx::vms::client::core {

QString RemoteConnectionFactoryContext::toString() const
{
    return nx::format("ConnectionContext to %1", logonData.address);
}

bool RemoteConnectionFactoryContext::isRestApiSupported() const
{
    if (!moduleInformation.version.isNull())
        return moduleInformation.version >= RemoteConnection::kRestApiSupportVersion;

    return logonData.expectedServerVersion
        && *logonData.expectedServerVersion >= RemoteConnection::kRestApiSupportVersion;
}

RemoteConnectionProcess::RemoteConnectionProcess(SystemContext* systemContext):
    context(new RemoteConnectionFactoryContext(systemContext))
{
    NX_ASSERT(systemContext);
}

RemoteConnectionProcess::~RemoteConnectionProcess()
{
    // Reset the context first to speed up an early return of async connection func.
    context->terminated = true;
    context.reset();
}

} // namespace nx::vms::client::core
