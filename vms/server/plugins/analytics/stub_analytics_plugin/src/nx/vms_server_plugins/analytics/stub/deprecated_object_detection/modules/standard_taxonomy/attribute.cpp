// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute.h"

#include <nx/kit/json.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

Attribute::Attribute(
    std::string name,
    std::string type,
    std::string subtype,
    SimpleOptional<int> minValue,
    SimpleOptional<int> maxValue,
    std::string unit)
    :
    name(std::move(name)),
    type(std::move(type)),
    subtype(std::move(subtype)),
    minValue(std::move(minValue)),
    maxValue(std::move(maxValue)),
    unit(std::move(unit))
{
}

nx::kit::Json Attribute::serialize() const
{
    using namespace nx::kit;
    Json::object json;
    json.insert(std::make_pair("name", name));
    json.insert(std::make_pair("type", type));
    if (!subtype.empty())
        json.insert(std::make_pair("subtype", subtype));

    if (minValue)
        json.insert(std::make_pair("minValue", *minValue));

    if (maxValue)
        json.insert(std::make_pair("maxValue", *maxValue));

    if (!unit.empty())
    json.insert(std::make_pair("unit", unit));

    return json;
}

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx