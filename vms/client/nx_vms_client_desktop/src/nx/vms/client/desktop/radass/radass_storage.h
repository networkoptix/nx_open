// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/radass/radass_types.h>

namespace nx::vms::client::desktop {

class AbstractRadassStorage
{
public:
    virtual ~AbstractRadassStorage() = default;

    virtual RadassModeByLayoutItemIdHash localModes() const = 0;
    virtual void setLocalModes(const RadassModeByLayoutItemIdHash& modes) = 0;

    virtual RadassModeByLayoutItemIdHash cloudModes() const = 0;
    virtual void setCloudModes(const RadassModeByLayoutItemIdHash& modes) = 0;
};

} // namespace nx::vms::client::desktop
