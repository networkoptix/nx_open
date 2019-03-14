#pragma once

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>
#include <nx/streaming/video_data_packet.h>

namespace nx {
namespace vms::server {
namespace analytics {

/**
 * Wrapper owning a QnCompressedVideoData.
 */
class CompressedVideoPacket:
    public nx::sdk::RefCountable<nx::sdk::analytics::ICompressedVideoPacket>
{
public:
    CompressedVideoPacket(QnConstCompressedVideoDataPtr frame):
        m_frame(std::move(frame)),
        m_codec(toString(m_frame->compressionType).toStdString())
    {
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
