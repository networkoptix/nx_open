#include "object_metadata.h"

namespace nx {
namespace sdk {
namespace analytics {

void* ObjectMetadata::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_ObjectMetadata)
    {
        addRef();
        return static_cast<IObjectMetadata*>(this);
    }
    if (interfaceId == IID_Metadata)
    {
        addRef();
        return static_cast<IMetadata*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

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

void ObjectMetadata::setAttributes(const std::vector<Attribute>& value)
{
    m_attributes = value;
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
