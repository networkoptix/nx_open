// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx {
namespace core {
namespace resource {

enum class DeviceType
{
    unknown = 0,
    camera,
    nvr,
    encoder,
    ioModule,
    hornSpeaker,
    multisensorCamera,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(DeviceType*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<DeviceType>;
    return visitor(
        Item{DeviceType::unknown, "Unknown"},
        Item{DeviceType::camera, "Camera"},
        Item{DeviceType::nvr, "NVR"},
        Item{DeviceType::encoder, "Encoder"},
        Item{DeviceType::ioModule, "IOModule"},
        Item{DeviceType::hornSpeaker, "HornSpeaker"},
        Item{DeviceType::multisensorCamera, "MultisensorCamera"}
    );
}

NX_VMS_COMMON_API bool isProxyDeviceType(DeviceType deviceType);

} // namespace resource
} // namespace core
} // namespace nx
