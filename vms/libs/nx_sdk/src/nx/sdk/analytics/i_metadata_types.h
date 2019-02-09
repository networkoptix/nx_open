#pragma once

#include <nx/sdk/interface.h>

#include <nx/sdk/i_string_list.h>

namespace nx {
namespace sdk {
namespace analytics {

class IMetadataTypes: public Interface<IMetadataTypes>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IMetadataTypes"); }

    virtual ~IMetadataTypes() = default;
    virtual const IStringList* eventTypeIds() const = 0;
    virtual const IStringList* objectTypeIds() const = 0;
    virtual bool isEmpty() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
