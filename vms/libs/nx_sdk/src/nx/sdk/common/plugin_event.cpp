#include "plugin_event.h"

namespace nx {
namespace sdk {
namespace common {

PluginEvent::PluginEvent(IPluginEvent::Level level, std::string caption, std::string description):
    m_level(level),
    m_caption(std::move(caption)),
    m_description(std::move(description))
{
}

void* PluginEvent::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nxpl::IID_PluginInterface)
        return this;

    if (interfaceId == nx::sdk::IID_PluginEvent)
        return this;

    return nullptr;
}

IPluginEvent::Level PluginEvent::level() const
{
    return m_level;
}

const char* PluginEvent::caption() const
{
    return m_caption.c_str();
}

const char* PluginEvent::description() const
{
    return m_description.c_str();
}

void PluginEvent::setLevel(IPluginEvent::Level level)
{
    m_level = level;
}

void PluginEvent::setCaption(std::string caption)
{
    m_caption = std::move(caption);
}

void PluginEvent::setDescription(std::string description)
{
    m_description = std::move(description);
}

} // namespace common
} // namespace sdk
} // namespace nx
