// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

#include <nx/kit/json.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

struct ColorType
{
    struct Item
    {
        std::string name;
        std::string rgb;
    };

    std::string id;
    std::string name;
    std::vector<Item> items;

    // TODO: Add inheritance.

    ColorType(
        std::string id,
        std::string name,
        std::vector<Item> items);

    nx::kit::Json serialize() const;
};

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx