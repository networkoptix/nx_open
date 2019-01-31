#pragma once

#include <nx/sdk/i_string_list.h>
#include <plugins/plugin_api.h>

namespace nx {
namespace sdk {
namespace analytics {

const nxpl::NX_GUID IID_MetadataTypes =
    {0x52,0x75,0xf1,0xa6,0x90,0xa7,0x41,0x52,0x8d,0x14,0xa7,0xa5,0xae,0x05,0xf4,0x8a};

class IMetadataTypes: public nxpl::PluginInterface
{
public:
    virtual ~IMetadataTypes() = default;
    virtual const IStringList* eventTypeIds() const = 0;
    virtual const IStringList* objectTypeIds() const = 0;
    virtual bool isEmpty() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
