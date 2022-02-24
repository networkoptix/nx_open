// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_type.h"

namespace nx::vms::api {

bool isProxyDeviceType(DeviceType deviceType)
{
    return deviceType == DeviceType::nvr
        || deviceType == DeviceType::encoder;
}

} // namespace nx::vms::api
