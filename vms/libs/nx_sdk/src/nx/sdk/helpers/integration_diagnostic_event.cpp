// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "integration_diagnostic_event.h"

#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

namespace nx::sdk {

IntegrationDiagnosticEvent::IntegrationDiagnosticEvent(
    Level level, std::string caption, std::string description)
    :
    m_level(level),
    m_caption(std::move(caption)),
    m_description(std::move(description))
{
}

IIntegrationDiagnosticEvent::Level IntegrationDiagnosticEvent::level() const
{
    return m_level;
}

const char* IntegrationDiagnosticEvent::caption() const
{
    return m_caption.c_str();
}

const char* IntegrationDiagnosticEvent::description() const
{
    return m_description.c_str();
}

void IntegrationDiagnosticEvent::setLevel(Level level)
{
    m_level = level;
}

void IntegrationDiagnosticEvent::setCaption(std::string caption)
{
    m_caption = std::move(caption);
}

void IntegrationDiagnosticEvent::setDescription(std::string description)
{
    m_description = std::move(description);
}

static std::string levelToString(IIntegrationDiagnosticEvent::Level level)
{
    using Level = IIntegrationDiagnosticEvent::Level;

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

std::string IntegrationDiagnosticEvent::toString() const
{
    static const std::string kIndent(4, ' ');
    return "{\n"
        + kIndent + "\"level\": " + nx::kit::utils::toString(levelToString(m_level)) + ",\n"
        + kIndent + "\"caption\": " + nx::kit::utils::toString(m_caption) + ",\n"
        + kIndent + "\"description\": " + nx::kit::utils::toString(m_description) + "\n"
        + "}";
}

} // namespace nx::sdk
