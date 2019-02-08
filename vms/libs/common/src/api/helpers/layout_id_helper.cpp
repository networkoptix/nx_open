#include "camera_id_helper.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

namespace nx {
namespace layout_id_helper {

QnLayoutResourcePtr findLayoutByFlexibleId(QnResourcePool* resourcePool, const QString& flexibleId)
{
    if (const QnUuid uuid = QnUuid::fromStringSafe(flexibleId); !uuid.isNull())
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

QnUuid flexibleIdToId(QnResourcePool* resourcePool, const QString& flexibleId)
{
    const auto layout = findLayoutByFlexibleId(resourcePool, flexibleId);
    return layout ? layout->getId() : QnUuid();
}

} // namespace layout_id_helper
} // namespace nx
