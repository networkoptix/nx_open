#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

struct ObjectClassDescription
{
    ObjectClassDescription(
        const std::string& name,
        const std::string& description,
        const std::string& typeId)
        :
        name(name),
        description(description),
        typeId(typeId)
    {
    }

    std::string name;
    std::string description;
    std::string typeId;
};

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
