#include "device_type.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace core {
namespace resource {

bool isProxyDeviceType(DeviceType deviceType)
{
    return deviceType == DeviceType::nvr
        || deviceType == DeviceType::encoder;
}

} // namespace resoruce
} // namespace core
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::core::resource, DeviceType,
    (nx::core::resource::DeviceType::unknown, "Unknown")
    (nx::core::resource::DeviceType::camera, "Camera")
    (nx::core::resource::DeviceType::nvr, "NVR")
    (nx::core::resource::DeviceType::encoder, "Encoder")
    (nx::core::resource::DeviceType::ioModule, "IOModule")
    (nx::core::resource::DeviceType::hornSpeaker, "HornSpeaker")
);