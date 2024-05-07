// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QString>

namespace nx::vms::client::desktop::manual_device_addition {

struct AddDeviceSearchAddress
{
    std::optional<QString> startAddress;
    std::optional<QString> endAddress;
    std::optional<int> port;
    int portIndex = -1;
    int portLength = -1;

    bool valid() const { return startAddress.has_value(); }
};

// Parse device addresses as required for the Add Device dialog.
AddDeviceSearchAddress NX_VMS_CLIENT_DESKTOP_API parseDeviceAddress(const QString& address);

} // namespace nx::vms::client::desktop
