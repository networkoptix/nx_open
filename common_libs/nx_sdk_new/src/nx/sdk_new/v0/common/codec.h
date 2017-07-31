#pragma once

namespace nx {
namespace sdk {
namespace v0 {

enum class Codec
{
    none,
    any,
    vendorSpecific,
    h264,
    h265,
    mjpeg

    // TODO: #dmishin add all needed codecs
};

using CodecTag = uint32_t;

} // namespace v0
} // namespace sdk
} // namespace nx
