#include "module_information_rest_handler.h"

#include <utils/network/tcp_connection_priv.h>
#include <utils/network/module_information.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include "version.h"

int QnModuleInformationRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    Q_UNUSED(path)
    Q_UNUSED(params)

    QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
    if (!server)
        return CODE_INTERNAL_ERROR;

    QnModuleInformation moduleInformation;
    moduleInformation.type = lit("Media Server");
    moduleInformation.customization = lit(QN_CUSTOMIZATION_NAME);
    moduleInformation.version = qnCommon->engineVersion();
    moduleInformation.systemInformation = QnSystemInformation(QN_APPLICATION_PLATFORM, QN_APPLICATION_ARCH, QN_ARM_BOX);
    moduleInformation.systemName = qnCommon->localSystemName();
    moduleInformation.id = qnCommon->moduleGUID();
    moduleInformation.isLocal = false;

    foreach (const QHostAddress &address, server->getNetAddrList())
        moduleInformation.remoteAddresses.insert(address.toString());

    moduleInformation.port = qnCommon->moduleUrl().port();

    result.setReply(moduleInformation);
    return CODE_OK;
}
