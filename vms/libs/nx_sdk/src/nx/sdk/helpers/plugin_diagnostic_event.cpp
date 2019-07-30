// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_diagnostic_event.h"

namespace nx {
namespace sdk {

PluginDiagnosticEvent::PluginDiagnosticEvent(Level level, std::string caption, std::string description):
    m_level(level),
    m_caption(std::move(caption)),
    m_description(std::move(description))
{
}

IPluginDiagnosticEvent::Level PluginDiagnosticEvent::level() const
{
    return m_level;
}

const char* PluginDiagnosticEvent::caption() const
{
    return m_caption.c_str();
}

const char* PluginDiagnosticEvent::description() const
{
    return m_description.c_str();
}

void PluginDiagnosticEvent::setLevel(Level level)
{
    m_level = level;
}

void PluginDiagnosticEvent::setCaption(std::string caption)
{
    m_caption = std::move(caption);
}

void PluginDiagnosticEvent::setDescription(std::string description)
{
    m_description = std::move(description);
}

} // namespace sdk
} // namespace nx
