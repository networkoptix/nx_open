#pragma once

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>
#include "abstract_compressed_video_packet.h"
#include "common_compressed_media_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

class CommonCompressedVideoPacket: public nxpt::CommonRefCounter<AbstractCompressedVideoPacket>
{
public:
    CommonCompressedVideoPacket() {}

    virtual const char* codec() const override { return m_mediaPacket.codec(); }
    virtual const int dataSize() const override { return m_mediaPacket.dataSize(); }
    virtual const char* data() const override { return m_mediaPacket.data(); }
    virtual const AbstractMediaContext* context() const override { return m_mediaPacket.context(); }

    virtual int width() const override { return m_width;  }
    virtual int height() const override { return m_height; }

    void setWidth(int value) { m_width = value; }
    void setHeight(int value) { m_height = value; }
    void setCodec(const std::string& value) { m_mediaPacket.setCodec(value); }
    void setData(std::vector<char> data) { m_mediaPacket.setData(data); }


    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

private:
    CommonCompressedMediaPacket m_mediaPacket;
    int m_width = 0;
    int m_height = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
