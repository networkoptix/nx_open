// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <optional>

#include <nx/network/socket_common.h>
#include <nx/utils/uuid.h>

class QnBaseSystemDescription;
using QnSystemDescriptionPtr = QSharedPointer<QnBaseSystemDescription>;

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API CloudSystemEndpoint
{
    QnUuid serverId;
    nx::network::SocketAddress address;
};

NX_VMS_CLIENT_CORE_API std::optional<CloudSystemEndpoint> cloudSystemEndpoint(
    const QString& systemId);

NX_VMS_CLIENT_CORE_API std::optional<CloudSystemEndpoint> cloudSystemEndpoint(
    const QnSystemDescriptionPtr& system);

} // namespace nx::vms::client::desktop
