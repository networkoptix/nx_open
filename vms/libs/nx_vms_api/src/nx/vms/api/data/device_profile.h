// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <string>

#include <nx/reflect/instrument.h>

namespace nx::vms::api {

struct DeviceProfile
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(State,
        none,
        primaryDefault,
        secondaryDefault
    );

    std::string token;
    std::string name;
    State state = State::none;
};
using DeviceProfiles = std::vector<DeviceProfile>;

#define DeviceProfile_Fields (token)(name)(state)
NX_REFLECTION_INSTRUMENT(DeviceProfile, DeviceProfile_Fields)

} // namespace nx::vms::api
