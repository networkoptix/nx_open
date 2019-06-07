#include "object_metadata.h"

#include <algorithm>

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace analytics {

const char* ObjectMetadata::typeId() const
{
    return m_typeId.c_str();
}

float ObjectMetadata::confidence() const
{
    return m_confidence;
}

Uuid ObjectMetadata::id() const
{
    return m_id;
}

const char* ObjectMetadata::subtype() const
{
    return m_subtype.data();
}

const IAttribute* ObjectMetadata::attribute(int index) const
{
    if (index >= (int) m_attributes.size() || index < 0)
        return nullptr;

    m_attributes[index]->addRef();
    return  m_attributes[index].get();
}

int ObjectMetadata::attributeCount() const
{
    return (int) m_attributes.size();
}

Rect ObjectMetadata::boundingBox() const
{
    return m_rect;
}

void ObjectMetadata::setTypeId(std::string typeId)
{
    m_typeId = std::move(typeId);
}

void ObjectMetadata::setConfidence(float confidence)
{
    m_confidence = confidence;
}

void ObjectMetadata::setId(const Uuid& value)
{
    m_id = value;
}

void ObjectMetadata::setSubtype(const std::string& value)
{
    m_subtype = value;
}

void ObjectMetadata::addAttribute(nx::sdk::Ptr<Attribute> attribute)
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

void ObjectMetadata::addAttributes(const std::vector<nx::sdk::Ptr<Attribute>>& value)
{
    for (const auto& newAttribute: value)
        addAttribute(newAttribute);
}

void ObjectMetadata::setBoundingBox(const Rect& rect)
{
    m_rect = rect;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
