#include "object.h"

namespace nx {
namespace sdk {
namespace analytics {

void* Object::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Object)
    {
        addRef();
        return static_cast<IObject*>(this);
    }
    if (interfaceId == IID_MetadataItem)
    {
        addRef();
        return static_cast<IMetadataItem*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

const char* Object::typeId() const
{
    return m_typeId.c_str();
}

float Object::confidence() const
{
    return m_confidence;
}

Uuid Object::id() const
{
    return m_id;
}

const char* Object::subtype() const
{
    return m_subtype.data();
}

const IAttribute* Object::attribute(int index) const
{
    return (index < (int) m_attributes.size()) ? &m_attributes[index] : nullptr;
}

int Object::attributeCount() const
{
    return (int) m_attributes.size();
}

IObject::Rect Object::boundingBox() const
{
    return m_rect;
}

const char* Object::auxiliaryData() const
{
    return m_auxiliaryData.c_str();
}

void Object::setTypeId(std::string typeId)
{
    m_typeId = std::move(typeId);
}

void Object::setConfidence(float confidence)
{
    m_confidence = confidence;
}

void Object::setId(const Uuid& value)
{
    m_id = value;
}

void Object::setSubtype(const std::string& value)
{
    m_subtype = value;
}

void Object::setAttributes(const std::vector<Attribute>& value)
{
    m_attributes = value;
}

void Object::setAuxiliaryData(std::string auxiliaryData)
{
    m_auxiliaryData = std::move(auxiliaryData);
}

void Object::setBoundingBox(const Rect& rect)
{
    m_rect = rect;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
