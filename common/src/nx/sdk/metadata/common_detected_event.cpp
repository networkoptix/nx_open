#include "common_detected_event.h"
#include <nx/utils/log/log_main.h>

namespace nx {
namespace sdk {
namespace metadata {

void* CommonDetectedEvent::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_DetectedEvent)
    {
        addRef();
        return static_cast<AbstractDetectedEvent*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}


nxpl::NX_GUID CommonDetectedEvent::typeId() const
{
    return m_typeId;
}

float CommonDetectedEvent::confidence() const
{
    return m_confidence;
}

const char* CommonDetectedEvent::caption() const
{
    return m_caption.c_str();
}

const char* CommonDetectedEvent::description() const
{
    return m_description.c_str();
}

const char* CommonDetectedEvent::auxilaryData() const
{
    return m_auxilaryData.c_str();
}

bool CommonDetectedEvent::isActive() const
{
    return m_isActive;
}

void CommonDetectedEvent::setTypeId(const nxpl::NX_GUID& typeId)
{
    m_typeId = typeId;
}

void CommonDetectedEvent::setConfidence(float confidence)
{
    m_confidence = confidence;
}

void CommonDetectedEvent::setCaption(const std::string& caption)
{
    m_caption = caption;
}

void CommonDetectedEvent::setDescription(const std::string& description)
{
    m_description = description;
}

void CommonDetectedEvent::setAuxilaryData(const std::string& auxilaryData)
{
    m_auxilaryData = auxilaryData;
}

void CommonDetectedEvent::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

CommonDetectedEvent::~CommonDetectedEvent()
{
    NX_VERBOSE(this, "DESTROYING ITEM");
}

} // namespace metadata
} // namespace sdk
} // namespace nx
