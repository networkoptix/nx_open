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
    return (index < (int) m_attributes.size()) ? &m_attributes[index] : nullptr;
}

int ObjectMetadata::attributeCount() const
{
    return (int) m_attributes.size();
}

IObjectMetadata::Rect ObjectMetadata::boundingBox() const
{
    return m_rect;
}

const char* ObjectMetadata::auxiliaryData() const
{
    return m_auxiliaryData.c_str();
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

void ObjectMetadata::addAttributes(const std::vector<Attribute>& value)
{
    for (const auto& newAttribute: value)
    {
        auto existingAttribute = std::find_if(m_attributes.begin(), m_attributes.end(),
            [newAttributeName = newAttribute.name()](const Attribute& attribute)
            {
                return strcmp(attribute.name(), newAttributeName) == 0;
            });

        if (existingAttribute != m_attributes.end())
        {
            NX_KIT_ASSERT(existingAttribute->type() == newAttribute.type());
            existingAttribute->setValue(newAttribute.value());
        }
        else
        {
            m_attributes.push_back(newAttribute);
        }
    }
}

void ObjectMetadata::setAuxiliaryData(std::string auxiliaryData)
{
    m_auxiliaryData = std::move(auxiliaryData);
}

void ObjectMetadata::setBoundingBox(const Rect& rect)
{
    m_rect = rect;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
