#include "validate_update_peer_task.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <api/model/update_information_reply.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <network/module_finder.h>
#include <utils/common/app_info.h>

QnValidateUpdatePeerTask::QnValidateUpdatePeerTask(QObject* parent):
    base_type(parent)
{
}

void QnValidateUpdatePeerTask::doStart()
{
    validateCloudHost();
}

void QnValidateUpdatePeerTask::doCancel()
{
}

void QnValidateUpdatePeerTask::validateCloudHost()
{
    const auto moduleFinder = QnModuleFinder::instance();
    if (!moduleFinder)
    {
        finish(ValidationFailed);
        return;
    }

    const auto cloudHost = QnAppInfo::defaultCloudHost();

    QSet<QnUuid> failedPeers;
    bool linkedToCloud = false;

    for (const auto& id: peers())
    {
        const auto server =
            qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        const auto moduleInformation = moduleFinder->moduleInformation(server);
        if (!moduleInformation.cloudSystemId.isEmpty())
            linkedToCloud = true;
        if (moduleInformation.cloudHost != cloudHost)
            failedPeers.insert(server->getId());
    }

    if (failedPeers.isEmpty() || !linkedToCloud)
        finish(NoError);
    else
        finish(CloudHostConflict, failedPeers);
}
