// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/vms/client/core/network/logon_data.h>

namespace nx::vms::client::desktop {

struct LogonData: core::LogonData
{
    /** The session should be stored into recent list and as a tile. */
    bool storeSession = true;

    /** Password should be stored. */
    bool storePassword = false;

    /** The client was run as a secondary instance - using "Open in New Window" action. */
    bool secondaryInstance = false;

    LogonData() = default;

    LogonData(const core::LogonData& logonData):
        core::LogonData(logonData)
    {
    }
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::LogonData)
