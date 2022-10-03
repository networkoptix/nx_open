// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_rights_types_testing.h"

#include <QtCore/QStringList>

#include <core/resource/resource.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/range_adapters.h>

namespace nx::vms::api {

void PrintTo(AccessRights accessRights, std::ostream* os)
{
    *os << nx::reflect::toString(accessRights);
}

} // namespace nx::vms::api

void PrintTo(const nx::core::access::ResourceAccessMap& map, std::ostream* os)
{
    QStringList lines;
    for (const auto& [id, accessRights]: nx::utils::constKeyValueRange(map))
        lines << nx::format("%1: %2", id.toSimpleString(), nx::reflect::toString(accessRights));

    lines.sort();
    *os << lines.join("\n").toStdString();
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
