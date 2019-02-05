#include "event_metadata.h"

namespace nx {
namespace sdk {
namespace analytics {

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

const char* EventMetadata::auxiliaryData() const
{
    return m_auxiliaryData.c_str();
}

bool EventMetadata::isActive() const
{
    return m_isActive;
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

void EventMetadata::setAuxiliaryData(const std::string& auxiliaryData)
{
    m_auxiliaryData = auxiliaryData;
}

void EventMetadata::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
