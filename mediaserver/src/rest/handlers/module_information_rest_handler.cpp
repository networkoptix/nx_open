#include "module_information_rest_handler.h"

#include <utils/network/tcp_connection_priv.h>
#include <utils/network/module_information.h>
#include <utils/network/networkoptixmodulerevealcommon.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include "version.h"


int QnModuleInformationRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    Q_UNUSED(path)
    Q_UNUSED(params)

    result.setReply(qnCommon->moduleInformation());
    return CODE_OK;
}
