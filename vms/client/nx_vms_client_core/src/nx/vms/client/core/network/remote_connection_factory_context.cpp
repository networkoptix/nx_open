// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "remote_connection_factory_context.h"

namespace nx::vms::client::core {

QString RemoteConnectionFactoryContext::toString() const
{
    return nx::format("ConnectionContext to %1", info.address);
}

} // namespace nx::vms::client::core
