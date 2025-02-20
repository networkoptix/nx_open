// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>

namespace nx::sdk::cloud_storage {

class IDataList
{
public:
    virtual void goToBeginning() const = 0;
    virtual void next() const = 0;
    virtual bool atEnd() const = 0;
};

} // namespace nx::sdk::cloud_storage
