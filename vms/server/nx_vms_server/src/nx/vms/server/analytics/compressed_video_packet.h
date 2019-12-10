#pragma once

#include <nx/streaming/video_data_packet.h>

#include <nx/sdk/helpers/list.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>

#include <nx/vms/server/analytics/motion_metadata_packet.h>

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
    CompressedVideoPacket(
        QnConstCompressedVideoDataPtr frame,
        QnConstMetaDataV1Ptr associatedMotionMetadata)
        :
        m_frame(std::move(frame)),
        m_codec(toString(m_frame->compressionType).toStdString()),
        m_metadataList(nx::sdk::makePtr<nx::sdk::List<nx::sdk::analytics::IMetadataPacket>>())
    {
        if (associatedMotionMetadata)
        {
            m_metadataList->addItem(
                nx::sdk::makePtr<MotionMetadataPacket>(associatedMotionMetadata).get());
        }
    }

    virtual int64_t timestampUs() const override { return m_frame->timestamp; }

    virtual const char* codec() const override { return m_codec.c_str(); }
    virtual const char* data() const override { return m_frame->data(); }
    virtual int dataSize() const override { return (int) m_frame->dataSize(); }

    virtual MediaFlags flags() const override
    {
        return (m_frame->flags & QnAbstractMediaData::MediaFlag::MediaFlags_AVKey)
            ? MediaFlags::keyFrame
            : MediaFlags::none;
    }

    virtual int width() const override { return m_frame->width; }
    virtual int height() const override { return m_frame->height; }

protected:
    virtual const nx::sdk::analytics::IMediaContext* getContext() const override
    {
        return nullptr;
    }

protected:
    virtual nx::sdk::IList<nx::sdk::analytics::IMetadataPacket>* getMetadataList() const
    {
        m_metadataList->addRef();
        return m_metadataList.get();
    }

private:
    const QnConstCompressedVideoDataPtr m_frame;
    const std::string m_codec;
    nx::sdk::Ptr<nx::sdk::List<nx::sdk::analytics::IMetadataPacket>> m_metadataList;
};

} // namespace analytics
} // namespace vms::server
} // namespace nx
