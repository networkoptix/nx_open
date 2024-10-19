// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../push_platform_helpers.h"

#include <UIKit/UiApplication.h>

#include "../push_notification_structures.h"

namespace nx::vms::client::mobile::details {

const DeviceInfo& getDeviceInfo()
{
    static const DeviceInfo result =
        []()
        {
            const auto device = [UIDevice currentDevice];
            return DeviceInfo{
                QString::fromNSString([device name]),
                QString::fromNSString([device model]),
                OsType::ios};
        }();
    return result;
}

void showOsPushSettingsScreen()
{
    const auto application = [UIApplication sharedApplication];
    const auto settingsUrl = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
    [application openURL:settingsUrl options: @{} completionHandler: nil];
}

} // namespace nx::vms::client::mobile::details
