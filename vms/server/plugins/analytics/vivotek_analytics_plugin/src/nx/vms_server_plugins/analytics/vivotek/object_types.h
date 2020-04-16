#pragma once

#include <string>

namespace nx::vms_server_plugins::analytics::vivotek {

struct ObjectTypes
{
    std::string person = "nx.vivotek.Person";
};

extern const ObjectTypes kObjectTypes;

} // namespace nx::vms_server_plugins::analytics::vivotek
