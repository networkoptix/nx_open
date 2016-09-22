#include "updates_common.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/module_finder.h>

QSet<QnUuid> QnUpdateUtils::getServersLinkedToCloud(const QSet<QnUuid>& peers)
{
    QSet<QnUuid> result;

    const auto moduleFinder = qnModuleFinder;
    if (!moduleFinder)
        return result;

    for (const auto& id: peers)
    {
        const auto server =
            qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        const auto moduleInformation = moduleFinder->moduleInformation(server);
        if (!moduleInformation.cloudSystemId.isEmpty())
            result.insert(id);
    }

    return result;
}
