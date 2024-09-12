// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_integration_diagnostic_event.h>

namespace nx::sdk {

class IntegrationDiagnosticEvent: public RefCountable<IIntegrationDiagnosticEvent>
{
public:
    IntegrationDiagnosticEvent(Level level, std::string caption, std::string description);

    virtual Level level() const override;
    virtual const char* caption() const override;
    virtual const char* description() const override;

    void setLevel(IIntegrationDiagnosticEvent::Level level);
    void setCaption(std::string caption);
    void setDescription(std::string description);

    /** Intended for logging. Produces a multi-line text without the trailing newline. */
    std::string toString() const;

private:
    Level m_level = Level::info;
    std::string m_caption;
    std::string m_description;
};

} // namespace nx::sdk
