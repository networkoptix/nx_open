// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_id_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

namespace nx {
namespace layout_id_helper {

QnLayoutResourcePtr findLayoutByFlexibleId(
    const QnResourcePool* resourcePool, const QString& flexibleId)
{
    if (const nx::Uuid uuid = nx::Uuid::fromStringSafe(flexibleId); !uuid.isNull())
        return resourcePool->getResourceById<QnLayoutResource>(uuid);

    if (const int logicalId = flexibleId.toInt(); logicalId > 0)
    {
        auto layoutsList = resourcePool->getResourcesByLogicalId(logicalId)
            .filtered<QnLayoutResource>();
        if (!layoutsList.isEmpty())
            return layoutsList.front();
    }
    return QnLayoutResourcePtr();
}

nx::Uuid flexibleIdToId(const QnResourcePool* resourcePool, const QString& flexibleId)
{
    const auto layout = findLayoutByFlexibleId(resourcePool, flexibleId);
    return layout ? layout->getId() : nx::Uuid();
}

} // namespace layout_id_helper
} // namespace nx
