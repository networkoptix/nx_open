// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <core/resource/resource_fwd.h>
#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::core {

/**
 * Helper class which enumerates a given list of servers one by one, trying all servers' addresses.
 */
struct NX_VMS_CLIENT_CORE_API ReconnectHelper: public SystemContextAware
{
    ReconnectHelper(SystemContext* systemContext, bool stickyReconnect = false);

    QnMediaServerResourcePtr currentServer() const;
    std::optional<nx::network::SocketAddress> currentAddress() const;

    /** Remove currently tried server from the list as it cannot be connected. */
    void markCurrentServerAsInvalid();

    /** Remove original (message bus) server from the list as it cannot be connected. */
    void markOriginalServerAsInvalid();

    void next();

    bool empty() const;

private:
    QnMediaServerResourceList m_servers;
    int m_currentIndex = -1;
    const QnUuid m_originalServerId;
};

} // namespace nx::vms::client::core
