// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_plugin.h>
#include <nx/sdk/cloud_storage/i_media_data_packet.h>
#include <nx/sdk/helpers/ref_countable.h>

namespace nx::sdk::cloud_storage {

struct MediaPacketData;

class MediaDataPacket: public nx::sdk::RefCountable<IMediaDataPacket>
{
public:
    MediaDataPacket(const MediaPacketData& d);
    virtual ~MediaDataPacket() override;
    virtual int64_t timestampUs() const override;
    virtual Type type() const override;
    virtual const void* data() const override;
    virtual unsigned int dataSize() const override;
    virtual unsigned int channelNumber() const override;
    virtual nxcip::CompressionType codecType() const override;
    virtual bool isKeyFrame() const override;
    virtual const void* encryptionData() const override;
    virtual int encryptionDataSize() const override;

private:
    std::unique_ptr<MediaPacketData> m_d;
};

} // namespace nx::sdk::cloud_storage
