// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/resource/server.h>

namespace nx::vms::client::core { class SystemContext; }

namespace nx::vms::client::desktop {

class CrossSystemServerResource: public nx::vms::client::core::ServerResource
{
public:
    /** Main server which was used for connection establishing. */
    CrossSystemServerResource(core::SystemContext* systemContext);

    /** Server for video direct access if possible. */
    CrossSystemServerResource(
        const nx::Uuid& id,
        nx::network::SocketAddress endpoint,
        core::SystemContext* systemContext);

    virtual QString rtspUrl() const override;
};

using CrossSystemServerResourcePtr = QnSharedResourcePointer<CrossSystemServerResource>;

} // namespace nx::vms::client::desktop
