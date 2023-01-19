// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_metadata.h"

#include <algorithm>

#include <nx/kit/debug.h>

namespace nx::sdk::analytics {

const char* ObjectMetadata::typeId() const
{
    return m_typeId.c_str();
}

float ObjectMetadata::confidence() const
{
    return m_confidence;
}

void ObjectMetadata::getTrackId(Uuid* outValue) const
{
    *outValue = m_trackId;
}

const char* ObjectMetadata::subtype() const
{
    return m_subtype.data();
}

const IAttribute* ObjectMetadata::getAttribute(int index) const
{
    if (index >= (int) m_attributes.size() || index < 0)
        return nullptr;

    return shareToPtr(m_attributes[index]).releasePtr();
}

int ObjectMetadata::attributeCount() const
{
    return (int) m_attributes.size();
}

void ObjectMetadata::getBoundingBox(Rect* outValue) const
{
    *outValue = m_rect;
}

void ObjectMetadata::setTypeId(std::string typeId)
{
    m_typeId = std::move(typeId);
}

void ObjectMetadata::setConfidence(float confidence)
{
    m_confidence = confidence;
}

void ObjectMetadata::setTrackId(const Uuid& value)
{
    m_trackId = value;
}

void ObjectMetadata::setSubtype(const std::string& value)
{
    m_subtype = value;
}

void ObjectMetadata::addAttribute(nx::sdk::Ptr<Attribute> attribute)
{
    if (!NX_KIT_ASSERT(attribute))
        return;

    m_attributes.push_back(std::move(attribute));
}

void ObjectMetadata::addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value)
{
    for (const auto& newAttribute: value)
        addAttribute(newAttribute);
}

void ObjectMetadata::addAttributes(std::vector<nx::sdk::Ptr<Attribute>>&& value)
{
    for (auto& newAttribute: value)
        addAttribute(std::move(newAttribute));
}

void ObjectMetadata::setBoundingBox(const Rect& rect)
{
    m_rect = rect;
}

} // namespace nx::sdk::analytics
