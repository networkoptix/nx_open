#pragma once

#include <nx/streaming/in_stream_compressed_metadata.h>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_custom_metadata_packet.h>

namespace nx::vms::server::analytics {

class CustomMetadataPacket: public nx::sdk::RefCountable<nx::sdk::analytics::ICustomMetadataPacket>
{
public:
    CustomMetadataPacket(
        std::shared_ptr<nx::streaming::InStreamCompressedMetadata> compressedMetadata);

    virtual const char* codec() const override;

    virtual const char* data() const override;

    virtual int dataSize() const override;

    virtual const char* contextData() const override;

    virtual int contextDataSize() const override;

    virtual int64_t timestampUs() const override;

private:
    const std::string m_codec;
    const std::shared_ptr<const nx::streaming::InStreamCompressedMetadata> m_metadata;
};

} // namespace nx::vms::server::analytics
