#include "pixel_format.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics, PixelFormat,
    (nx::vms::api::analytics::PixelFormat::yuv420, "yuv420")
    (nx::vms::api::analytics::PixelFormat::argb, "argb")
    (nx::vms::api::analytics::PixelFormat::abgr, "abgr")
    (nx::vms::api::analytics::PixelFormat::rgba, "rgba")
    (nx::vms::api::analytics::PixelFormat::bgra, "bgra")
    (nx::vms::api::analytics::PixelFormat::rgb, "rgb")
    (nx::vms::api::analytics::PixelFormat::bgr, "bgr")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::analytics::PixelFormat, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics, PixelFormats,
    (nx::vms::api::analytics::PixelFormat::yuv420, "yuv420")
    (nx::vms::api::analytics::PixelFormat::argb, "argb")
    (nx::vms::api::analytics::PixelFormat::abgr, "abgr")
    (nx::vms::api::analytics::PixelFormat::rgba, "rgba")
    (nx::vms::api::analytics::PixelFormat::bgra, "bgra")
    (nx::vms::api::analytics::PixelFormat::rgb, "rgb")
    (nx::vms::api::analytics::PixelFormat::bgr, "bgr")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::analytics::PixelFormats, (numeric)(debug))
