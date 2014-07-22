#include "module_information_rest_handler.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

#include <utils/network/tcp_connection_priv.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include "version.h"

int QnModuleInformationRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    Q_UNUSED(path)
    Q_UNUSED(params)

    QJsonObject json;

    json.insert(lit("type"), lit("Media Server"));
    json.insert(lit("customization"), lit(QN_CUSTOMIZATION_NAME));
    json.insert(lit("version"), QnSoftwareVersion(lit(QN_ENGINE_VERSION)).toString());
    json.insert(lit("systemInformation"), QnSystemInformation(QN_APPLICATION_PLATFORM, QN_APPLICATION_ARCH, QN_ARM_BOX).toString());
    json.insert(lit("systemName"), qnCommon->localSystemName());
    json.insert(lit("id"), qnCommon->moduleGUID().toString());

    result.setReply(QJsonValue(json));
    return CODE_OK;
}
