#include "common_event.h"

namespace nx {
namespace sdk {
namespace analytics {

void* CommonEvent::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Event)
    {
        addRef();
        return static_cast<Event*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

const char* CommonEvent::typeId() const
{
    return m_typeId.c_str();
}

float CommonEvent::confidence() const
{
    return m_confidence;
}

const char* CommonEvent::caption() const
{
    return m_caption.c_str();
}

const char* CommonEvent::description() const
{
    return m_description.c_str();
}

const char* CommonEvent::auxilaryData() const
{
    return m_auxilaryData.c_str();
}

bool CommonEvent::isActive() const
{
    return m_isActive;
}

void CommonEvent::setTypeId(std::string typeId)
{
    m_typeId = std::move(typeId);
}

void CommonEvent::setConfidence(float confidence)
{
    m_confidence = confidence;
}

void CommonEvent::setCaption(const std::string& caption)
{
    m_caption = caption;
}

void CommonEvent::setDescription(const std::string& description)
{
    m_description = description;
}

void CommonEvent::setAuxilaryData(const std::string& auxilaryData)
{
    m_auxilaryData = auxilaryData;
}

void CommonEvent::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

CommonEvent::~CommonEvent()
{
}

} // namespace analytics
} // namespace sdk
} // namespace nx
