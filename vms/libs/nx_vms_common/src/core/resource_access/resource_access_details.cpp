// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_details.h"

#include <core/resource/resource.h>
#include <nx/utils/log/format.h>
#include <nx/utils/range_adapters.h>

namespace nx::core::access {

ResourceAccessDetails& operator+=(
    ResourceAccessDetails& destination, const ResourceAccessDetails& source)
{
    if (destination.empty())
    {
        destination = source;
    }
    else
    {
        for (const auto& [subjectId, resources]: nx::utils::constKeyValueRange(source))
            destination[subjectId] += resources;
    }

    return destination;
}

void PrintTo(const nx::core::access::ResourceAccessDetails& details, std::ostream* os)
{
    QStringList lines;
    for (const auto& [subjectId, resources]: nx::utils::constKeyValueRange(details))
    {
        QStringList names;
        for (const auto& resource: resources)
            names.push_back(resource ? resource->getName() : "NULL");

        names.sort();
        lines << nx::format("%1: {%2}", subjectId.toSimpleString(), names.join(", "));
    }

    lines.sort();
    *os << lines.join("\n").toStdString();
}

} // namespace nx::core::access
