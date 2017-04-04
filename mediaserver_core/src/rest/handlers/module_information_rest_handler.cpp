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

namespace {
    QSet<QString> getAddresses(const QnMediaServerResourcePtr &server)
    {
        const auto port = server->getPort();
        QSet<QString> result;
        for (const SocketAddress& address: server->getAllAvailableAddresses())
        {
            if (address.port == port)
                result << address.address.toString();
            else
                result << address.toString();
        }

        return result;
    }
}

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
        if (!QnPermissionsHelper::hasOwnerPermissions(owner->accessRights()))
            return QnPermissionsHelper::notOwnerError(result);
    }

    if (allModules)
    {
        const auto allServers = owner->resourcePool()->getAllServers(Qn::AnyStatus);
        if (useAddresses)
        {
            QList<QnModuleInformationWithAddresses> modules;
            for (const QnMediaServerResourcePtr &server : allServers)
            {
                QnModuleInformationWithAddresses moduleInformation = server->getModuleInformation();
                moduleInformation.remoteAddresses = getAddresses(server);
                modules.append(std::move(moduleInformation));
            }
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
        QnModuleInformationWithAddresses moduleInformation(owner->commonModule()->moduleInformation());
        QnMediaServerResourcePtr server = owner->resourcePool()->getResourceById<QnMediaServerResource>(owner->commonModule()->moduleGUID());
        if (server)
            moduleInformation.remoteAddresses = getAddresses(server);
        result.setReply(moduleInformation);
    }
    else
    {
        result.setReply(owner->commonModule()->moduleInformation());
    }
    return CODE_OK;
}
