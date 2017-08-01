#pragma once

#include <plugins/metadata/abstract_media_frame.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::GUID IID_AudioFrame
    = {{0x02, 0x98, 0x8d, 0xda, 0x71, 0x55, 0x49, 0x85, 0xb9, 0x7e, 0xbd, 0x8f, 0xcb, 0x43, 0x26, 0x66}};

class AbstractCompressedAudioFrame: public AbstractMediaFrame
{
public:
    // ???
};

} // namespace metadata
} // namespace sdk
} // namespace nx
