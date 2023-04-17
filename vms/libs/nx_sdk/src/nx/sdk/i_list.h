// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "interface.h"

namespace nx::sdk {

template<typename IItem>
class IList: public Interface<IList<IItem>>
{
public:
    static auto interfaceId()
    {
        return IList::template makeIdForTemplate<IList<IItem>, IItem>("nx::sdk::IList");
    }

    virtual int count() const = 0;

    /** Called by at() */
    protected: virtual IItem* getAt(int index) const = 0;
    /** @return Element at the zero-based index, or null if the index is invalid. */
    public: Ptr<IItem> at(int index) const { return Ptr(getAt(index)); }
};
template<typename IItem>
using IList0 = IList<IItem>;

} // namespace nx::sdk
