#include "module_information_rest_handler.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/tcp_connection_priv.h>
#include <network/module_information.h>
#include <nx/fusion/model_functions.h>
#include <common/common_module.h>
#include <rest/helpers/permissions_helper.h>
#include <core/resource_management/resource_pool.h>
#include <rest/server/rest_connection_processor.h>

#include <nx/network/socket_common.h>

int QnModuleInformationRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path)

    bool allModules = params.value(lit("allModules")) == lit("true");
    bool useAddresses = params.value(lit("showAddresses"), lit("true")) != lit("false");
    bool checkOwnerPermissions = params.value(lit("checkOwnerPermissions"), lit("false")) != lit("false");

    if (checkOwnerPermissions)
    {
        if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
            return QnPermissionsHelper::notOwnerError(result);
    }

    if (allModules)
    {
        const auto allServers = owner->resourcePool()->getAllServers(Qn::AnyStatus);
        if (useAddresses)
        {
            QList<QnModuleInformationWithAddresses> modules;
            for (const QnMediaServerResourcePtr &server : allServers)
                modules.append(std::move(server->getModuleInformationWithAddresses()));

            result.setReply(modules);
        }
        else
        {
            QList<QnModuleInformation> modules;
            for (const QnMediaServerResourcePtr &server : allServers)
                modules.append(server->getModuleInformation());
            result.setReply(modules);
        }
    }
    else if (useAddresses)
    {
        const auto id = owner->commonModule()->moduleGUID();
        if (const auto s = owner->resourcePool()->getResourceById<QnMediaServerResource>(id))
            result.setReply(s->getModuleInformationWithAddresses());
        else
            result.setReply(owner->commonModule()->moduleInformation());
    }
    else
    {
        result.setReply(owner->commonModule()->moduleInformation());
    }
    return CODE_OK;
}
