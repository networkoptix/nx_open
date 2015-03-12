#include "module_information_rest_handler.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/tcp_connection_priv.h>
#include <utils/network/module_information.h>
#include <utils/common/model_functions.h>
#include <common/common_module.h>

int QnModuleInformationRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)

    bool allModules = params.value(lit("allModules")) == lit("true");

    if (allModules) {
        QList<QnModuleInformation> modules;
        for (const QnMediaServerResourcePtr &server: qnResPool->getAllServers())
            modules.append(server->getModuleInformation());
        result.setReply(modules);
    } else {
        result.setReply(qnCommon->moduleInformation());
    }
    return CODE_OK;
}
