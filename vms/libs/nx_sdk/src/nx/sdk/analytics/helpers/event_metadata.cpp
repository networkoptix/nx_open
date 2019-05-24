#include "event_metadata.h"

#include <nx/kit/debug.h>

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

const IAttribute* EventMetadata::attribute(int index) const
{
    if (index >= (int) m_attributes.size() || index < 0)
        return nullptr;

    m_attributes[index]->addRef();
    return  m_attributes[index].get();
}

int EventMetadata::attributeCount() const
{
    return (int) m_attributes.size();
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

void EventMetadata::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

void EventMetadata::addAttribute(nx::sdk::Ptr<Attribute> attribute)
{
    if (!NX_KIT_ASSERT(attribute))
        return;

    auto existingAttribute = std::find_if(m_attributes.begin(), m_attributes.end(),
        [attributeName = attribute->name()](const nx::sdk::Ptr<Attribute>& attribute)
    {
        return strcmp(attribute->name(), attributeName) == 0;
    });

    if (existingAttribute != m_attributes.end())
    {
        NX_KIT_ASSERT((*existingAttribute)->type() == attribute->type());
        (*existingAttribute)->setValue(attribute->value());
    }
    else
    {
        m_attributes.push_back(std::move(attribute));
    }
}

void EventMetadata::addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value)
{
    for (const auto& newAttribute : value)
        addAttribute(newAttribute);
}

} // namespace analytics
} // namespace sdk
} // namespace nx
