// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_factory_context.h"

namespace nx::vms::client::core {

const nx::utils::SoftwareVersion RemoteConnectionFactoryContext::kRestApiSupportVersion(5, 0);

QString RemoteConnectionFactoryContext::toString() const
{
    return nx::format("ConnectionContext to %1", logonData.address);
}

bool RemoteConnectionFactoryContext::isRestApiSupported() const
{
    if (!moduleInformation.version.isNull())
        return moduleInformation.version >= kRestApiSupportVersion;

    return logonData.expectedServerVersion
        && *logonData.expectedServerVersion >= kRestApiSupportVersion;
}

RemoteConnectionProcess::RemoteConnectionProcess():
    context(new RemoteConnectionFactoryContext())
{
}

RemoteConnectionProcess::~RemoteConnectionProcess()
{
    // Reset the context first to speed up an early return of async connection func.
    context->terminated = true;
    context.reset();
}

} // namespace nx::vms::client::core
