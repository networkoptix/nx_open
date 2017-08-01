#pragma once

#include <plugins/plugin_api.h>
#include <plugins/metadata/abstract_media_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

static const nxpl::GUID IID_CompressedVideo
    = {{0xb6, 0x39, 0xe4, 0x68, 0x0d, 0x95, 0x49, 0x76, 0xa7, 0xc3, 0x68, 0x4b, 0xcc, 0x4d, 0x90, 0xb9}};

class AbstractCompressedVideoData: public AbstractMediaPacket
{
public:
    virtual int width() const = 0;
    virtual int height() const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
