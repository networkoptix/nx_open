// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <optional>

#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API CloudSystemEndpoint
{
    QnUuid serverId;
    nx::network::SocketAddress address;
};

NX_VMS_CLIENT_CORE_API std::optional<CloudSystemEndpoint> cloudSystemEndpoint(
    const QString& systemId);

} // namespace nx::vms::client::desktop
