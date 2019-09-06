// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {

/**
 * VMS Event which can be triggered by a Plugin to inform the System about some issue or status
 * change.
 */
class IPluginDiagnosticEvent: public nx::sdk::Interface<IPluginDiagnosticEvent>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::IPluginDiagnosticEvent"); }

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

} // namespace sdk
} // namespace nx
