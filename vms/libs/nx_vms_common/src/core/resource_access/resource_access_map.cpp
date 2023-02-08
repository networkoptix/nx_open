// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_access_map.h"

#include <QtCore/QStringList>

#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/format.h>
#include <nx/utils/range_adapters.h>
#include <nx/vms/api/data/access_rights_data.h>

using namespace nx::vms::api;

namespace nx::core::access {

ResourceAccessMap kAdminResourceAccessMap{
    {kAllDevicesGroupId, kFullAccessRights},
    {kAllWebPagesGroupId, kViewAccessRights},
    {kAllServersGroupId, kViewAccessRights},
    {kAllVideowallsGroupId, kFullAccessRights}};

ResourceAccessMap& operator+=(ResourceAccessMap& destination, const ResourceAccessMap& source)
{
    if (destination.empty())
    {
        destination = source;
    }
    else
    {
        for (const auto& [resource, permissions]: nx::utils::constKeyValueRange(source))
            destination[resource] |= permissions;
    }

    return destination;
}

QString toString(const ResourceAccessMap& accessMap, QnResourcePool* resourcePool, bool multiLine)
{
    if (accessMap.empty())
        return "{}";

    QStringList result;
    const QString lineTemplate = multiLine ? "    {%1: %2}" : "{%1: %2}";

    const auto resourceName =
        [resourcePool](const QnUuid& resourceId) -> QString
        {
            if (const auto specialGroup = nx::vms::api::specialResourceGroup(resourceId))
                return nx::format("%1", *specialGroup).toQString();

            const auto resource = resourcePool
                ? resourcePool->getResourceById(resourceId)
                : QnResourcePtr();

            return resource
                ? nx::format("\"%1\"", resource->getName()).toQString()
                : resourceId.toSimpleString();
        };

    for (const auto& [resourceId, accessRights]: nx::utils::constKeyValueRange(accessMap))
        result.push_back(nx::format(lineTemplate, resourceName(resourceId), accessRights));

    result.sort();

    return multiLine
        ? nx::format("{\n%1\n}", result.join(",\n"))
        : nx::format("{%1}", result.join(", "));
}

} // namespace nx::core::access
