#pragma once

#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {

/**
* Each class that implements IString interface should properly handle this GUID in
* its queryInterface method.
*/
static const nxpl::NX_GUID IID_String =
    {{0xbe,0x00,0x06,0x82,0x93,0xef,0x4b,0x5f,0x85,0x37,0xe5,0x07,0x16,0x26,0x9c,0xf9}};

class IString: public nxpl::PluginInterface
{
public:
    virtual const char* str() const = 0;
};

} // namespace sdk
} // namespace nx
