#pragma once

namespace nx {
namespace sdk {
namespace v0 {

enum class PixelFormat
{
    none,
    any,
    vendorSpecific,
    argb32,
    argb32Premultiplied,
    rgb32,
    rgb24,
    rgb565,
    rgb555,
    argb8565Premultiplied,
    bgra32,
    bgra32Premultiplied,
    bgr32,
    bgr24,
    bgr565,
    bgr555,
    bgr5658Premultiplied,
    ayuv444,
    yuv444,
    yub420p,
    yv12,
    uyvy,
    yuyv,
    nv12,
    nv21,
    imc1,
    imc2,
    imc3,
    imc4,
    y8,
    y16,
    jpeg,
    cameraRaw,
    adobeDng
};

} // namespace v0
} // namespace sdk
} // namespace nx
