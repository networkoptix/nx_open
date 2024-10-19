// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../push_platform_helpers.h"

#include <QtCore/QJniObject>
#include <QtCore/QJniEnvironment>

#include "../push_notification_structures.h"

namespace nx::vms::client::mobile::details {

const DeviceInfo& getDeviceInfo()
{
    static const auto kDeviceInfo =
        []()
        {
            const auto callDeviceInfoFunction =
                [](const char* function) -> QString
                {
                    static constexpr auto kDeviceInfoClassName =
                        "com/nxvms/mobile/utils/DeviceInfo";

                    return QJniObject::callStaticObjectMethod<jstring>(
                        kDeviceInfoClassName, function).toString();
                };

            return DeviceInfo{
                callDeviceInfoFunction("name"),
                callDeviceInfoFunction("model"),
                OsType::android};
        }();

    return kDeviceInfo;
}

void showOsPushSettingsScreen()
{
    static const auto kUtilsClass = "com/nxvms/mobile/utils/QnWindowUtils";
    QJniObject::callStaticMethod<void>(kUtilsClass, "showOsPushSettingsScreen");
}

} // namespace nx::vms::client::mobile::details
