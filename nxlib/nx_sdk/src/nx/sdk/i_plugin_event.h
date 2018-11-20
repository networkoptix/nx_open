#pragma once

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {

/**
 * Each class that implements IPluginEvent interface should properly handle this GUID in
 * its queryInterface method.
 */
static const nxpl::NX_GUID IID_PluginEvent =
    {{0x34,0x2c,0x8e,0x8b,0xad,0x94,0x49,0x85,0x90,0xd8,0x60,0x47,0x0c,0xb9,0x4b,0x9c}};

/**
 * VMS Event which can be triggered by a Plugin to inform the System about some issue or status
 * change.
 */
class IPluginEvent: public nxpl::PluginInterface
{
public:
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
