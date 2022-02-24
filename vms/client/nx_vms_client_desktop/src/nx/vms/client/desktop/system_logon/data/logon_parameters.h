// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QMetaType>

#include <nx/vms/client/core/network/connection_info.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct LogonParameters
{
    /** Target server address and credentials. */
    core::ConnectionInfo connectionInfo;

    /** The session should be stored into recent list and as a tile. */
    bool storeSession = true;

    /** Password should be stored. */
    bool storePassword = false;

    /** The client was run as a secondary instance - using "Open in New Window" action. */
    bool secondaryInstance = false;

    LogonParameters() = default;

    LogonParameters(core::ConnectionInfo connectionInfo):
        connectionInfo(std::move(connectionInfo))
    {
    }
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::LogonParameters)
