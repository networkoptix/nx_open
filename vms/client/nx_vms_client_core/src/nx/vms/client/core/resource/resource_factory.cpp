// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_factory.h"

#include "server.h"

namespace nx::vms::client::core {

QnMediaServerResourcePtr ResourceFactory::createServer() const
{
    return QnMediaServerResourcePtr(new ServerResource());
}

} // namespace nx::vms::client::core
