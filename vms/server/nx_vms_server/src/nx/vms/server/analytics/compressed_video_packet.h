#pragma once

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>
#include <nx/streaming/video_data_packet.h>

namespace nx {
namespace vms::server {
namespace analytics {

/**
 * Wrapper owning a QnCompressedVideoData.
 */
class CompressedVideoPacket:
    public nxpt::CommonRefCounter<nx::sdk::analytics::ICompressedVideoPacket>
{
public:
    CompressedVideoPacket(QnConstCompressedVideoDataPtr frame):
        m_frame(std::move(frame)),
        m_codec(toString(m_frame->compressionType).toStdString())
    {
    }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override
    {
        if (interfaceId == nx::sdk::analytics::IID_CompressedVideoPacket)
        {
            addRef();
            return static_cast<ICompressedVideoPacket*>(this);
        }
        if (interfaceId == nx::sdk::analytics::IID_CompressedMediaPacket)
        {
            addRef();
            return static_cast<ICompressedMediaPacket*>(this);
        }
        if (interfaceId == nx::sdk::analytics::IID_CompressedMediaPacket)
        {
            addRef();
            return static_cast<ICompressedMediaPacket*>(this);
        }
        if (interfaceId == nx::sdk::analytics::IID_DataPacket)
        {
            addRef();
            return static_cast<IDataPacket*>(this);
        }
        if (interfaceId == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    virtual int64_t timestampUs() const override { return m_frame->timestamp; }

    virtual const char* codec() const override { return m_codec.c_str(); }
    virtual const char* data() const override { return m_frame->data(); }
    virtual int dataSize() const override { return (int) m_frame->dataSize(); }
    virtual const nx::sdk::analytics::IMediaContext* context() const override { return nullptr; }

    virtual MediaFlags flags() const override
    {
        return (m_frame->flags & QnAbstractMediaData::MediaFlag::MediaFlags_AVKey)
            ? MediaFlags::keyFrame
            : MediaFlags::none;
    }

    virtual int width() const override { return m_frame->width; }
    virtual int height() const override { return m_frame->height; }

private:
    const QnConstCompressedVideoDataPtr m_frame;
    const std::string m_codec;
};

} // namespace analytics
} // namespace vms::server
} // namespace nx
