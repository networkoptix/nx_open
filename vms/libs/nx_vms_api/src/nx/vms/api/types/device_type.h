// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

enum class DeviceType
{
    /**apidoc
     * %caption Unknown
     */
    unknown = 0,

    /**apidoc
     * %caption Camera
     */
    camera,

    /**apidoc
     * %caption NVR
     */
    nvr,

    /**apidoc
     * %caption Encoder
     */
    encoder,

    /**apidoc
     * %caption IOModule
     */
    ioModule,

    /**apidoc
     * %caption HornSpeaker
     */
    hornSpeaker,

    /**apidoc
     * %caption MultisensorCamera
     */
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

NX_VMS_API bool isProxyDeviceType(DeviceType deviceType);

} // namespace nx::vms::api
