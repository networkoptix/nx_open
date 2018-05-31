#pragma once

#include <string>

#include <plugins/plugin_api.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

struct ObjectClassDescription
{
    ObjectClassDescription(
        const std::string& name,
        const std::string& description,
        const nxpl::NX_GUID& guid)
        :
        name(name),
        description(description),
        guid(guid)
    {
    }

    std::string name;
    std::string description;
    nxpl::NX_GUID guid;
};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
