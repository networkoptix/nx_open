// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timestamped_object_metadata.h"

#include <nx/kit/debug.h>

namespace nx::sdk::analytics {

TimestampedObjectMetadata::TimestampedObjectMetadata():
    m_objectMetadata(nx::sdk::makePtr<ObjectMetadata>())
{
}

const char* TimestampedObjectMetadata::typeId() const
{
    return m_objectMetadata->typeId();
}

float TimestampedObjectMetadata::confidence() const
{
    return m_objectMetadata->confidence();
}

void TimestampedObjectMetadata::getTrackId(Uuid* outValue) const
{
    *outValue = m_objectMetadata->trackId();
}

const char* TimestampedObjectMetadata::subtype() const
{
    return m_objectMetadata->subtype();
}

const IAttribute* TimestampedObjectMetadata::getAttribute(int index) const
{
    if (index >= m_objectMetadata->attributeCount() || index < 0)
        return nullptr;

    const Ptr<const IAttribute> attribute = m_objectMetadata->attribute(index);
    if (!NX_KIT_ASSERT(attribute))
        return nullptr;

    return shareToPtr(attribute).releasePtr();
}

int TimestampedObjectMetadata::attributeCount() const
{
    return m_objectMetadata->attributeCount();
}

void TimestampedObjectMetadata::getBoundingBox(Rect* outValue) const
{
    *outValue = m_objectMetadata->boundingBox();
}

int64_t TimestampedObjectMetadata::timestampUs() const
{
    return m_timestampUs;
}

void TimestampedObjectMetadata::setTypeId(std::string typeId)
{
    m_objectMetadata->setTypeId(std::move(typeId));
}

void TimestampedObjectMetadata::setConfidence(float confidence)
{
    m_objectMetadata->setConfidence(confidence);
}

void TimestampedObjectMetadata::setTrackId(const Uuid& value)
{
    m_objectMetadata->setTrackId(std::move(value));
}

void TimestampedObjectMetadata::setSubtype(const std::string& value)
{
    m_objectMetadata->setSubtype(std::move(value));
}

void TimestampedObjectMetadata::addAttribute(nx::sdk::Ptr<Attribute> attribute)
{
    m_objectMetadata->addAttribute(std::move(attribute));
}

void TimestampedObjectMetadata::addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value)
{
    m_objectMetadata->addAttributes(value);
}

void TimestampedObjectMetadata::setBoundingBox(const Rect& rect)
{
    m_objectMetadata->setBoundingBox(std::move(rect));
}

void TimestampedObjectMetadata::setTimestampUs(int64_t timestamp)
{
    m_timestampUs = timestamp;
}

} // namespace nx::sdk::analytics
