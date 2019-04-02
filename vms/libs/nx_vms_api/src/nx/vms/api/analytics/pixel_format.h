#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::analytics {

enum class PixelFormat
{
    undefined = 0,
    yuv420 = 1 << 0,
    argb = 1 << 1,
    abgr = 1 << 2,
    rgba = 1 << 3,
    bgra = 1 << 4,
    rgb = 1 << 5,
    bgr = 1 << 6,
};

Q_DECLARE_FLAGS(PixelFormats, PixelFormat);

Q_DECLARE_OPERATORS_FOR_FLAGS(PixelFormats)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PixelFormat)

} // namespace nx::vms::api::analytics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::PixelFormat,
    (metatype)(numeric)(lexical), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::PixelFormats,
    (metatype)(numeric)(lexical), NX_VMS_API)
