#include "change_system_name_rest_handler.h"

#include <nx_ec/ec_api.h>
#include <nx_ec/dummy_handler.h>
#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <media_server/settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

namespace HttpStatusCode {
    enum Code {
        ok = 200,
        badRequest = 400
    };
}

int QnChangeSystemNameRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(path)
    Q_UNUSED(contentType)
    Q_UNUSED(result)

    QString systemName = params.value("systemName");
    if (qnCommon->localSystemName() == systemName)
        return HttpStatusCode::ok;

    bool wholeSystem = params.contains("wholeSystem");

    QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
    if (!server) {
        NX_LOG("Cannot find self server resource!", cl_logERROR);
        return HttpStatusCode::badRequest;
    }

    MSSettings::roSettings()->setValue("systemName", systemName);
    qnCommon->setLocalSystemName(systemName);

    server->setSystemName(systemName);
    QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->save(server, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    if (wholeSystem)
        QnAppServerConnectionFactory::getConnection2()->getMiscManager()->changeSystemName(systemName, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    return HttpStatusCode::ok;
}

int QnChangeSystemNameRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) {
    Q_UNUSED(body)

    return executeGet(path, params, result, contentType);
}
