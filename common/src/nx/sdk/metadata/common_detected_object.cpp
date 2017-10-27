#include "common_detected_object.h"

namespace nx {
namespace sdk {
namespace metadata {


CommonDetectedObject::~CommonDetectedObject()
{
}

void* CommonDetectedObject::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_DetectedObject)
    {
        addRef();
        return static_cast<AbstarctDetectedObject*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

nxpl::NX_GUID CommonDetectedObject::eventTypeId() const
{
    return m_eventTypeId;
}

float CommonDetectedObject::confidence() const
{
    return m_confidence;
}

nxpl::NX_GUID CommonDetectedObject::id() const
{
    return m_id;
}

NX_ASCII const char* CommonDetectedObject::objectSubType() const
{
    return m_objectSubType.data();
}

NX_LOCALE_DEPENDENT const Attribute* CommonDetectedObject::attribute(int index) const
{
    return index < m_attributes.size() ? &m_attributes[index] : nullptr;
}

int CommonDetectedObject::attributeCount() const
{
    return m_attributes.size();
}

Rect CommonDetectedObject::boundingBox() const
{
    return m_rect;
}

const char* CommonDetectedObject::auxilaryData() const
{
    return m_auxilaryData.c_str();
}

void CommonDetectedObject::setEventTypeId(const nxpl::NX_GUID& eventTypeId)
{
    m_eventTypeId = eventTypeId;
}

void CommonDetectedObject::setConfidence(float confidence)
{
    m_confidence = confidence;
}

void CommonDetectedObject::setId(const nxpl::NX_GUID& value)
{
    m_id = value;
}

void CommonDetectedObject::setObjectSubType(const std::string& value)
{
    m_objectSubType = value;
}

void CommonDetectedObject::seAttributes(const std::vector<NX_LOCALE_DEPENDENT Attribute>& value)
{
    m_attributes = value;
}

void CommonDetectedObject::setAuxilaryData(const std::string& auxilaryData)
{
    m_auxilaryData = auxilaryData;
}

void CommonDetectedObject::setBoundingBox(const Rect& rect)
{
    m_rect = rect;
}

} // namespace metadata
} // namespace sdk
} // namespace nx
