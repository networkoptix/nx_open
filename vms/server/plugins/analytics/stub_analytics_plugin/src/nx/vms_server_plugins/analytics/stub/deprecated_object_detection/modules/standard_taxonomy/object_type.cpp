// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_type.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

ObjectType::ObjectType(
    std::string id,
    std::string name,
    std::string icon,
    std::string base,
    std::vector<Attribute> attributes)
    :
    id(std::move(id)),
    name(std::move(name)),
    icon(std::move(icon)),
    base(std::move(base)),
    attributes(std::move(attributes))
{
}

nx::kit::Json ObjectType::serialize() const
{
    using namespace nx::kit;

    Json::object json;
    json.insert(std::make_pair("id", id));
    json.insert(std::make_pair("name", name));
    json.insert(std::make_pair("icon", icon));
    json.insert(std::make_pair("base", base));

    Json::array attributeArray;
    for (const Attribute& attribute: attributes)
        attributeArray.push_back(attribute.serialize());

    json.insert(std::make_pair("attributes", std::move(attributeArray)));

    return json;
}

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx