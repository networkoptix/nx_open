// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_string_list.h>
#include <nx/sdk/interface.h>

namespace nx::sdk::analytics {

class IMetadataTypes: public Interface<IMetadataTypes>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IMetadataTypes"); }

    virtual ~IMetadataTypes() = default;

    /** Called by eventTypeIds() */
    protected: virtual const IStringList* getEventTypeIds() const = 0;
    public: Ptr<const IStringList> eventTypeIds() const { return Ptr(getEventTypeIds()); }

    /** Called by objectTypeIds() */
    protected: virtual const IStringList* getObjectTypeIds() const = 0;
    public: Ptr<const IStringList> objectTypeIds() const { return Ptr(getObjectTypeIds()); }

    virtual bool isEmpty() const = 0;
};
using IMetadataTypes0 = IMetadataTypes;

} // namespace nx::sdk::analytics
