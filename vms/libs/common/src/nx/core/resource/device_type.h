#pragma once

#include <nx/fusion/model_functions_fwd.h>

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
    hornSpeaker
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(DeviceType)

bool isProxyDeviceType(DeviceType deviceType);

} // namespace resource
} // namespace core
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::core::resource::DeviceType, (numeric)(lexical));
