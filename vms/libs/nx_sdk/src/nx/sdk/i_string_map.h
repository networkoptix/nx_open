#pragma once

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {

/**
 * Each class that implements IStringMap interface should properly handle this GUID in
 * its queryInterface method.
 */
static const nxpl::NX_GUID IID_StringMap =
    {{0x48,0x5a,0x23,0x51,0x55,0x73,0x4f,0xb5,0xa9,0x11,0xe4,0xfb,0x22,0x87,0x79,0x24}};

class IStringMap: public nxpl::PluginInterface
{
public:
    virtual int count() const = 0;

    /** @return Null if the index is invalid. */
    virtual const char* key(int i) const = 0;

    /** @return Null if the index is invalid. */
    virtual const char* value(int i) const = 0;

    /** @return Null if not found or key is null. */
    virtual const char* value(const char* key) const = 0;
};

} // namespace sdk
} // namespace nx
