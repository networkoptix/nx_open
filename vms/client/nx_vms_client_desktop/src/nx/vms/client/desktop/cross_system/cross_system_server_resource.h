// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/network/remote_connection_fwd.h>
#include <nx/vms/client/core/resource/server.h>

namespace nx::vms::client::desktop {

class CrossSystemServerResource: public nx::vms::client::core::ServerResource
{
public:
    /** Main server which was used for connection establishing. */
    CrossSystemServerResource(core::RemoteConnectionPtr connection);

    /** Server for video direct access if possible. */
    CrossSystemServerResource(
        const QnUuid& id,
        nx::network::SocketAddress endpoint,
        core::RemoteConnectionPtr connection);

    virtual QString rtspUrl() const override;
};

using CrossSystemServerResourcePtr = QnSharedResourcePointer<CrossSystemServerResource>;

} // namespace nx::vms::client::desktop
