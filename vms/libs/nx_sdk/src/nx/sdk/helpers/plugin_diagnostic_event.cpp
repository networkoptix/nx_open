// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "plugin_diagnostic_event.h"

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

namespace nx::sdk {

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

static std::string levelToString(IPluginDiagnosticEvent::Level level)
{
    using Level = IPluginDiagnosticEvent::Level;

    switch (level)
    {
        case Level::info: return "info";
        case Level::warning: return "warning";
        case Level::error: return "error";
        default:
            NX_KIT_ASSERT(false);
            return nx::kit::utils::format("unknown(%d)", (int) level);
    }
}

std::string PluginDiagnosticEvent::toString() const
{
    static const std::string kIndent(4, ' ');
    return "{\n"
        + kIndent + "\"level\": " + nx::kit::utils::toString(levelToString(m_level)) + ",\n"
        + kIndent + "\"caption\": " + nx::kit::utils::toString(m_caption) + ",\n"
        + kIndent + "\"description\": " + nx::kit::utils::toString(m_description) + "\n"
        + "}";
}

} // namespace nx::sdk
