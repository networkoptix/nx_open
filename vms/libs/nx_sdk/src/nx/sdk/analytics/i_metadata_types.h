// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    protected: virtual const IStringList* getEventTypeIds() const = 0;
    public: Ptr<const IStringList> eventTypeIds() const { return toPtr(getEventTypeIds()); }

    protected: virtual const IStringList* getObjectTypeIds() const = 0;
    public: Ptr<const IStringList> objectTypeIds() const { return toPtr(getObjectTypeIds()); }

    virtual bool isEmpty() const = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
