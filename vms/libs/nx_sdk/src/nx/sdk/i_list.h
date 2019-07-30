// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_object_metadata.h>

namespace nx {
namespace sdk {

template<typename IItem>
class IList: public Interface<IList<IItem>>
{
public:
    static auto interfaceId()
    {
        return InterfaceId::makeForTemplate<IList<IItem>, IItem>("nx::sdk::IList");
    }

    virtual int count() const = 0;

    /** @return Element at the zero-based index, or null if the index is invalid. */
    virtual IItem* at(int index) const = 0;
};

} // namespace sdk
} // namespace nx
