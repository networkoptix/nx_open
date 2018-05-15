#pragma once

#include <nx/sdk/metadata/compressed_video_packet.h>

#include "generic_compressed_media_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

class GenericCompressedVideoPacket: public GenericCompressedMediaPacket<CompressedVideoPacket>
{
public:
    GenericCompressedVideoPacket() {}
    virtual int width() const override { return m_width;  }
    virtual int height() const override { return m_height; }

    void setWidth(int value) { m_width = value; }
    void setHeight(int value) { m_height = value; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

private:
    int m_width = 0;
    int m_height = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
