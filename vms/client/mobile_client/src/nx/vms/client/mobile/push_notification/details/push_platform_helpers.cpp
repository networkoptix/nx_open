// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_platform_helpers.h"

#include <QtCore/QtGlobal>

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)

#include "push_notification_structures.h"

namespace nx::vms::client::mobile::details {

const DeviceInfo& getDeviceInfo()
{
    static const DeviceInfo kTestDeviceInfo{
        "Desktop Name",
        "Desktop Model",
        OsType::genericDesktop};

    return kTestDeviceInfo;
}

void showOsPushSettingsScreen()
{
}

} // namespace nx::vms::client::mobile::details

#endif // !defined(Q_OS_IOS)
