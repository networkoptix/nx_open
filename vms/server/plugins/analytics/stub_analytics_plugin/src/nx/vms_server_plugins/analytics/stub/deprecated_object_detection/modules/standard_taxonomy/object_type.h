// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/json.h>

#include "attribute.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

struct ObjectType
{
    std::string id;
    std::string name;
    std::string icon;
    std::string base;
    std::vector<Attribute> attributes;

    // TODO: Add inheritance.

    ObjectType(
        std::string id,
        std::string name,
        std::string icon,
        std::string base,
        std::vector<Attribute> attributes);

    nx::kit::Json serialize() const;
};

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx