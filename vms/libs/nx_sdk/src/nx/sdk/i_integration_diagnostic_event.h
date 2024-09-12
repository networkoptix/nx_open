// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk {

/**
 * VMS Event which can be triggered by a Plugin to inform the System about some issue or status
 * change.
 */
class IIntegrationDiagnosticEvent: public nx::sdk::Interface<IIntegrationDiagnosticEvent>
{
public:
    static auto interfaceId()
    {
        return makeIdWithAlternative("nx::sdk::IIntegrationDiagnosticEvent",
            /* Old name from 6.0. */ "nx::sdk::IPluginDiagnosticEvent");
    }

    enum class Level
    {
        info,
        warning,
        error,
    };

    virtual Level level() const = 0;
    virtual const char* caption() const = 0;
    virtual const char* description() const = 0;
};
using IIntegrationDiagnosticEvent0 = IIntegrationDiagnosticEvent;

} // namespace nx::sdk
