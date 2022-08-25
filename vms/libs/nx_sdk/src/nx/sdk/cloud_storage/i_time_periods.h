// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk::cloud_storage {

/**
 * Abstract representation of time period list. Each time period is defined by its beginning and
 * end.
 */
class ITimePeriods: public nx::sdk::Interface<ITimePeriods>
{
public:
    virtual void goToBeginning() const = 0;
    virtual bool next() const = 0;
    virtual bool atEnd() const = 0;
    virtual bool get(int64_t* outStartUs, int64_t* outEndUs) const = 0;
};

} // namespace nx::sdk::cloud_storage
