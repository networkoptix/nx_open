#include "module_information_rest_handler.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/tcp_connection_priv.h>
#include <network/module_information.h>
#include <utils/common/model_functions.h>
#include <common/common_module.h>

#include <nx/network/socket_common.h>

namespace {
    QSet<QString> getAddresses(const QnMediaServerResourcePtr &server)
    {
        const auto port = server->getPort();
        QSet<QString> result;
        for (const SocketAddress& address : server->getAllAvailableAddresses())
        {
            //TODO: #dklyckov why are we filtering addresses by port here?
            if (address.port != port)
                continue;

            result << address.address.toString();
        }
        return result;
    }
}

int QnModuleInformationRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)

    bool allModules = params.value(lit("allModules")) == lit("true");
    bool useAddresses = params.value(lit("showAddresses"), lit("true")) != lit("false");

    if (allModules)
    {
        const auto allServers = qnResPool->getAllServers(Qn::AnyStatus);
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
        QnModuleInformationWithAddresses moduleInformation(qnCommon->moduleInformation());
        QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
        if (server)
            moduleInformation.remoteAddresses = getAddresses(server);
        result.setReply(moduleInformation);
    }
    else
    {
        result.setReply(qnCommon->moduleInformation());
    }
    return CODE_OK;
}
