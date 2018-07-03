#include "camera_id_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

namespace nx {
namespace layout_id_helper {

QnLayoutResourcePtr findLayoutByFlexibleId(QnResourcePool* resourcePool, const QString& flexibleId)
{
    QnResourcePtr result;
    const QnUuid uuid = QnUuid::fromStringSafe(flexibleId);
    if (!uuid.isNull())
        result = resourcePool->getResourceById(uuid);
    if (!result)
    {
        auto resourceList = resourcePool->getResourcesByLogicalId(flexibleId.toInt());
        if (!resourceList.isEmpty())
            result = resourceList.front();
    }

    // If the found resource is not a camera, return null.
    return result.dynamicCast<QnLayoutResource>();
}

QnUuid flexibleIdToId(QnResourcePool* resourcePool, const QString& flexibleId)
{
    const auto layout = findLayoutByFlexibleId(resourcePool, flexibleId);
    return layout ? layout->getId() : QnUuid();
}

} // namespace layout_id_helper
} // namespace nx
