// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "enum_type.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

EnumType::EnumType(
    std::string id,
    std::string name,
    std::vector<std::string> items)
    :
    id(std::move(id)),
    name(std::move(name)),
    items(std::move(items))
{
}

nx::kit::Json EnumType::serialize() const
{
    using namespace nx::kit;
    Json::object json;
    json.insert(std::make_pair("id", id));
    json.insert(std::make_pair("name", name));

    Json::array itemsArray;
    for (const std::string& item: items)
        itemsArray.emplace_back(Json(item));

    json.insert(std::make_pair("items", itemsArray));

    return json;
}

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx