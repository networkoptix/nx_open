// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/reflect/enum_instrument.h>
#include <nx/vms/client/core/network/logon_data.h>

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

    /** Connecting from System Tab Bar. */
    connectFromTabBar,

    /** Connecting using the Command Line parameters. */
    connectUsingCommand,

    /** Merging from the Main Menu dialog. */
    mergeFromDialog,

    /** Merging from the Resource Tree context menu. */
    mergeFromTree
);

struct LogonData: core::LogonData
{
    /** The session should be stored into recent list and as a tile. */
    bool storeSession = true;

    /** Password should be stored. */
    bool storePassword = false;

    /** The client was run as a secondary instance - using "Open in New Window" action. */
    bool secondaryInstance = false;

    /** Connect scenario. */
    std::optional<ConnectScenario> connectScenario;

    LogonData() = default;

    LogonData(const core::LogonData& logonData):
        core::LogonData(logonData)
    {
    }

    bool operator==(const LogonData& other) const
    {
        return this->address == other.address
            && this->credentials == other.credentials;
    }
};

struct CloudSystemConnectData
{
    QString systemId;
    std::optional<ConnectScenario> connectScenario;
};

} // namespace nx::vms::client::desktop
