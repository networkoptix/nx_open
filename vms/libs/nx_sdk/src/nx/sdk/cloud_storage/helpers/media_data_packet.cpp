// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_data_packet.h"

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>

#include "data.h"

namespace nx::sdk::cloud_storage {

MediaDataPacket::MediaDataPacket(const MediaPacketData& d):
    m_d(new MediaPacketData(d))
{
}

MediaDataPacket::~MediaDataPacket()
{
}

int64_t MediaDataPacket::timestampUs() const
{
    return m_d->timestampUs;
}

IMediaDataPacket::Type MediaDataPacket::type() const
{
    return m_d->type;
}

const void* MediaDataPacket::data() const
{
    return m_d->data.data();
}

unsigned int MediaDataPacket::dataSize() const
{
    return m_d->dataSize;
}

unsigned int MediaDataPacket::channelNumber() const
{
    return m_d->channelNumber;
}

nxcip::CompressionType MediaDataPacket::codecType() const
{
    return m_d->compressionType;
}

bool MediaDataPacket::isKeyFrame() const
{
    return m_d->isKeyFrame;
}

const void* MediaDataPacket::encryptionData() const
{
    return m_d->encryptionData.data();
}

int MediaDataPacket::encryptionDataSize() const
{
    return m_d->encryptionData.size();
}

} // namespace nx::sdk::cloud_storage
