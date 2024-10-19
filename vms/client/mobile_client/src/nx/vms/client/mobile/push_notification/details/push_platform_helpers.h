// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::vms::client::mobile::details {

struct DeviceInfo;
const DeviceInfo& getDeviceInfo();

void showOsPushSettingsScreen();

} // namespace nx::vms::client::mobile::details
