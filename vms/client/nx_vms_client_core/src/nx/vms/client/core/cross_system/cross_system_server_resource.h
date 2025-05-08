// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/server.h>

namespace nx::vms::client::core {

class SystemContext;

class NX_VMS_CLIENT_CORE_API CrossSystemServerResource: public ServerResource
{
public:
    /** Main server which was used for connection establishing. */
    explicit CrossSystemServerResource(SystemContext* systemContext);

    /** Server for video direct access if possible. */
    CrossSystemServerResource(
        const nx::Uuid& id,
        nx::network::SocketAddress endpoint,
        SystemContext* systemContext);

    virtual QString rtspUrl() const override;
};

} // namespace nx::vms::client::core
