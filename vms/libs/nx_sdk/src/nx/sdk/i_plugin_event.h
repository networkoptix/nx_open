// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx {
namespace sdk {

/**
 * VMS Event which can be triggered by a Plugin to inform the System about some issue or status
 * change.
 */
class IPluginEvent: public nx::sdk::Interface<IPluginEvent>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::IPluginEvent"); }

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
