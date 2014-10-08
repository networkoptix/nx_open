#include "module_information_rest_handler.h"

#include <utils/network/tcp_connection_priv.h>
#include <utils/network/module_information.h>
#include <common/common_module.h>

int QnModuleInformationRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    result.setReply(qnCommon->moduleInformation());
    return CODE_OK;
}
