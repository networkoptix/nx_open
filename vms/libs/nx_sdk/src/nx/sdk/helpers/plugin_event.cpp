#include "plugin_event.h"

namespace nx {
namespace sdk {

PluginEvent::PluginEvent(Level level, std::string caption, std::string description):
    m_level(level),
    m_caption(std::move(caption)),
    m_description(std::move(description))
{
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

void PluginEvent::setLevel(Level level)
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

} // namespace sdk
} // namespace nx
