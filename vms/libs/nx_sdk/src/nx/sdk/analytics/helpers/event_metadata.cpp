// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_metadata.h"

#include <algorithm>

#include <nx/kit/debug.h>

namespace nx::sdk::analytics {

const char* EventMetadata::typeId() const
{
    return m_typeId.c_str();
}

float EventMetadata::confidence() const
{
    return m_confidence;
}

const char* EventMetadata::caption() const
{
    return m_caption.c_str();
}

const char* EventMetadata::description() const
{
    return m_description.c_str();
}

const IAttribute* EventMetadata::getAttribute(int index) const
{
    if (index >= (int) m_attributes.size() || index < 0)
        return nullptr;

    return shareToPtr(m_attributes[index]).releasePtr();
}

int EventMetadata::attributeCount() const
{
    return (int) m_attributes.size();
}

bool EventMetadata::isActive() const
{
    return m_isActive;
}

const char* EventMetadata::key() const
{
    return m_key.c_str();
}

void EventMetadata::setTypeId(std::string typeId)
{
    m_typeId = std::move(typeId);
}

void EventMetadata::setConfidence(float confidence)
{
    m_confidence = confidence;
}

void EventMetadata::setCaption(const std::string& caption)
{
    m_caption = caption;
}

void EventMetadata::setDescription(const std::string& description)
{
    m_description = description;
}

void EventMetadata::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

void EventMetadata::setKey(std::string key)
{
    m_key = std::move(key);
}

void EventMetadata::addAttribute(nx::sdk::Ptr<Attribute> attribute)
{
    if (!NX_KIT_ASSERT(attribute))
        return;

    m_attributes.push_back(std::move(attribute));
}

void EventMetadata::addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value)
{
    for (const auto& newAttribute: value)
        addAttribute(newAttribute);
}

void EventMetadata::setTrackId(Uuid trackId)
{
    m_trackId = trackId;
}

void EventMetadata::getTrackId(Uuid* outValue) const
{
    if (!NX_KIT_ASSERT(outValue))
        return;

    *outValue = m_trackId;
}

} // namespace nx::sdk::analytics
