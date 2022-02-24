// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <core/resource/resource_fwd.h>
#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

namespace nx::vms::client::core {

struct ReconnectHelper: public CommonModuleAware
{
    ReconnectHelper(std::optional<QnUuid> stickyReconnectTo = std::nullopt);

    QnMediaServerResourcePtr currentServer() const;
    std::optional<nx::network::SocketAddress> currentAddress() const;

    /**
     * Remove currently tried server from the list as it cannot be connected.
     */
    void markCurrentServerAsInvalid();

    void next();

    bool empty() const;

private:
    QnMediaServerResourceList m_servers;
    int m_currentIndex = -1;
};

} // namespace nx::vms::client::core

