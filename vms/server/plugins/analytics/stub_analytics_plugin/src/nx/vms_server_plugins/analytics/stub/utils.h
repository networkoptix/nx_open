// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

bool toBool(std::string str);

template<typename T>
T clamp(const T& value, const T& lowerBound, const T& upperBound)
{
    if (value < lowerBound)
        return lowerBound;

    if (value > upperBound)
        return upperBound;

    return value;
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
