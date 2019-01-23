#include "event.h"

namespace nx {
namespace sdk {
namespace analytics {

void* Event::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Event)
    {
        addRef();
        return static_cast<IEvent*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

const char* Event::typeId() const
{
    return m_typeId.c_str();
}

float Event::confidence() const
{
    return m_confidence;
}

const char* Event::caption() const
{
    return m_caption.c_str();
}

const char* Event::description() const
{
    return m_description.c_str();
}

const char* Event::auxiliaryData() const
{
    return m_auxiliaryData.c_str();
}

bool Event::isActive() const
{
    return m_isActive;
}

void Event::setTypeId(std::string typeId)
{
    m_typeId = std::move(typeId);
}

void Event::setConfidence(float confidence)
{
    m_confidence = confidence;
}

void Event::setCaption(const std::string& caption)
{
    m_caption = caption;
}

void Event::setDescription(const std::string& description)
{
    m_description = description;
}

void Event::setAuxiliaryData(const std::string& auxiliaryData)
{
    m_auxiliaryData = auxiliaryData;
}

void Event::setIsActive(bool isActive)
{
    m_isActive = isActive;
}

Event::~Event()
{
}

} // namespace analytics
} // namespace sdk
} // namespace nx
