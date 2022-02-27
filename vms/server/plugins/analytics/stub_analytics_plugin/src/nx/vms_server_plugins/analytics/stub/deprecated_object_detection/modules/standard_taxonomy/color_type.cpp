// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "color_type.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

ColorType::ColorType(
    std::string id,
    std::string name,
    std::vector<Item> items)
    :
    id(std::move(id)),
    name(std::move(name)),
    items(std::move(items))
{
}

nx::kit::Json ColorType::serialize() const
{
    using namespace nx::kit;

    Json::object json;

    json.insert(std::make_pair("id", id));
    json.insert(std::make_pair("name", name));

    Json::array itemArray;
    for (const Item& item: items)
    {
        Json::object itemObject;
        itemObject.insert(std::make_pair("name", item.name));
        itemObject.insert(std::make_pair("rgb", item.rgb));

        itemArray.push_back(std::move(itemObject));
    }

    json.insert(std::make_pair("items", std::move(itemArray)));

    return json;
}

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx