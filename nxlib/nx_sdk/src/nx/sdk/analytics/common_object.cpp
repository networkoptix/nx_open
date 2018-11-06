#include "common_object.h"

namespace nx {
namespace sdk {
namespace analytics {

CommonObject::~CommonObject()
{
}

void* CommonObject::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Object)
    {
        addRef();
        return static_cast<Object*>(this);
    }
    if (interfaceId == IID_MetadataItem)
    {
        addRef();
        return static_cast<MetadataItem*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

const char* CommonObject::typeId() const
{
    return m_typeId.c_str();
}

float CommonObject::confidence() const
{
    return m_confidence;
}

nxpl::NX_GUID CommonObject::id() const
{
    return m_id;
}

NX_ASCII const char* CommonObject::objectSubType() const
{
    return m_objectSubType.data();
}

NX_LOCALE_DEPENDENT const Attribute* CommonObject::attribute(int index) const
{
    return (index < (int) m_attributes.size()) ? &m_attributes[index] : nullptr;
}

int CommonObject::attributeCount() const
{
    return (int) m_attributes.size();
}

Rect CommonObject::boundingBox() const
{
    return m_rect;
}

const char* CommonObject::auxilaryData() const
{
    return m_auxilaryData.c_str();
}

void CommonObject::setTypeId(std::string typeId)
{
    m_typeId = std::move(typeId);
}

void CommonObject::setConfidence(float confidence)
{
    m_confidence = confidence;
}

void CommonObject::setId(const nxpl::NX_GUID& value)
{
    m_id = value;
}

void CommonObject::setObjectSubType(const std::string& value)
{
    m_objectSubType = value;
}

void CommonObject::setAttributes(const std::vector<CommonAttribute>& value)
{
    m_attributes = value;
}

void CommonObject::setAuxilaryData(std::string auxilaryData)
{
    m_auxilaryData = std::move(auxilaryData);
}

void CommonObject::setBoundingBox(const Rect& rect)
{
    m_rect = rect;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
