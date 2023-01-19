// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_track_best_shot_packet.h"

#include <algorithm>

#include <nx/kit/debug.h>

namespace nx::sdk::analytics {

ObjectTrackBestShotPacket::ObjectTrackBestShotPacket(
    Uuid trackId,
    int64_t timestampUs,
    Rect boundingBox)
    :
    m_trackId(trackId),
    m_timestampUs(timestampUs),
    m_boundingBox(boundingBox)
{
}

void ObjectTrackBestShotPacket::getTrackId(Uuid* outValue) const
{
    *outValue = m_trackId;
}

ObjectTrackBestShotPacket::Flags ObjectTrackBestShotPacket::flags() const
{
    return m_flags;
}

int64_t ObjectTrackBestShotPacket::timestampUs() const
{
    return m_timestampUs;
}

void ObjectTrackBestShotPacket::getBoundingBox(Rect* outValue) const
{
    *outValue = m_boundingBox;
}

const char* ObjectTrackBestShotPacket::imageUrl() const
{
    return m_imageUrl.c_str();
}

const char* ObjectTrackBestShotPacket::imageData() const
{
    return m_imageData.data();
}

int ObjectTrackBestShotPacket::imageDataSize() const
{
    return (int) m_imageData.size();
}

const char* ObjectTrackBestShotPacket::imageDataFormat() const
{
    return m_imageDataFormat.c_str();
}

void ObjectTrackBestShotPacket::setTrackId(const Uuid& trackId)
{
    m_trackId = trackId;
}

void ObjectTrackBestShotPacket::setFlags(Flags flags)
{
    m_flags = flags;
}

void ObjectTrackBestShotPacket::setTimestampUs(int64_t timestampUs)
{
    m_timestampUs = timestampUs;
}

void ObjectTrackBestShotPacket::setBoundingBox(const Rect& boundingBox)
{
    m_boundingBox = boundingBox;
}

void ObjectTrackBestShotPacket::setImageUrl(std::string imageUrl)
{
    m_imageUrl = std::move(imageUrl);
}

void ObjectTrackBestShotPacket::setImageDataFormat(std::string imageDataFormat)
{
    m_imageDataFormat = std::move(imageDataFormat);
}

void ObjectTrackBestShotPacket::setImageData(std::vector<char> imageData)
{
    m_imageData = std::move(imageData);
}

void ObjectTrackBestShotPacket::setImage(std::string imageDataFormat, std::vector<char> imageData)
{
    setImageDataFormat(std::move(imageDataFormat));
    setImageData(std::move(imageData));
}

const IAttribute* ObjectTrackBestShotPacket::getAttribute(int index) const
{
    if (index >= (int) m_attributes.size() || index < 0)
        return nullptr;

    return shareToPtr(m_attributes[index]).releasePtr();
}

int ObjectTrackBestShotPacket::attributeCount() const
{
    return (int) m_attributes.size();
}

void ObjectTrackBestShotPacket::addAttribute(Ptr<Attribute> attribute)
{
    if (!NX_KIT_ASSERT(attribute))
        return;

    m_attributes.push_back(std::move(attribute));
}

void ObjectTrackBestShotPacket::addAttributes(const std::vector<Ptr<Attribute>>& value)
{
    for (const auto& newAttribute: value)
        addAttribute(newAttribute);
}

} // namespace nx::sdk::analytics
