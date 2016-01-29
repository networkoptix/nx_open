#include "module_information_rest_handler.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/tcp_connection_priv.h>
#include <network/module_information.h>
#include <utils/common/model_functions.h>
#include <common/common_module.h>

namespace {
    QSet<QString> getAddresses(const QnMediaServerResourcePtr &server) {
        QSet<QString> addresses;
        QSet<QString> ignoredHosts;
        for (const QUrl &url: server->getIgnoredUrls())
            ignoredHosts.insert(url.host());

        const auto port = server->getPort();
        for (const auto& address: server->getNetAddrList()) {
            if (address.port == port) {
                QString addressString = address.address.toString();
                if (!ignoredHosts.contains(addressString))
                    addresses.insert(addressString);
            }
        }
        for (const QUrl &url: server->getAdditionalUrls()) {
            if (!ignoredHosts.contains(url.host()))
                addresses.insert(url.host());
        }
        return addresses;
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
            for (const QnMediaServerResourcePtr &server: allServers)
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
            for (const QnMediaServerResourcePtr &server: allServers)
                modules.append(server->getModuleInformation());
            result.setReply(modules);
        }
    } else if (useAddresses) {
        QnModuleInformationWithAddresses moduleInformation(qnCommon->moduleInformation());
        QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
        if (server)
            moduleInformation.remoteAddresses = getAddresses(server);
        result.setReply(moduleInformation);
    } else {
        result.setReply(qnCommon->moduleInformation());
    }
    return CODE_OK;
}
