// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/kit/json.h>

#include <nx/vms_server_plugins/analytics/stub/utils.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

struct Attribute
{
    std::string name;
    std::string type;
    std::string subtype;
    SimpleOptional<int> minValue;
    SimpleOptional<int> maxValue;
    std::string unit;

    Attribute() = default;

    Attribute(
        std::string name,
        std::string type,
        std::string subtype = std::string(),
        SimpleOptional<int> minValue = SimpleOptional<int>(),
        SimpleOptional<int> maxValue = SimpleOptional<int>(),
        std::string unit = std::string());

    nx::kit::Json serialize() const;
};

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx