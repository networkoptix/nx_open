#pragma once

#include <string>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
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
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
