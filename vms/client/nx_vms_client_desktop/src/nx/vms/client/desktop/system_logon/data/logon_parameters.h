// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QMetaType>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/network/connection_info.h>

namespace nx::vms::client::desktop {

NX_REFLECTION_ENUM_CLASS(ConnectScenario,
    /** Auto-connect on startup. */
    autoConnect,

    /** Connecting from the Login Dialog. */
    connectFromDialog,

    /** Connecting from the Welcome Screen tile. */
    connectFromTile,

    /** Connecting from the Resource Tree context menu. */
    connectFromTree,

    /** Connecting using the Command Line parameters. */
    connectUsingCommand,

    /** Merging from the Main Menu dialog. */
    mergeFromDialog,

    /** Merging from the Resource Tree context menu. */
    mergeFromTree
);

struct LogonParameters
{
    /** Target server address and credentials. */
    core::ConnectionInfo connectionInfo;

    /** Target server id. */
    std::optional<QnUuid> expectedServerId;

    /** The session should be stored into recent list and as a tile. */
    bool storeSession = true;

    /** Password should be stored. */
    bool storePassword = false;

    /** The client was run as a secondary instance - using "Open in New Window" action. */
    bool secondaryInstance = false;

    /** Connect scenario. */
    std::optional<ConnectScenario> connectScenario;

    LogonParameters() = default;

    LogonParameters(core::ConnectionInfo connectionInfo):
        connectionInfo(std::move(connectionInfo))
    {
    }
};

struct CloudSystemConnectData
{
    QString systemId;
    std::optional<ConnectScenario> connectScenario;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::LogonParameters)
Q_DECLARE_METATYPE(nx::vms::client::desktop::CloudSystemConnectData)
