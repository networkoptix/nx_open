#include "join_system_rest_handler.h"

#include <QtCore/QRegExp>

#include "core/resource_management/resource_pool.h"
#include "core/resource/user_resource.h"
#include "core/resource/media_server_resource.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/dummy_handler.h"
#include "api/app_server_connection.h"
#include "common/common_module.h"
#include "media_server/settings.h"
#include "media_server/serverutil.h"
#include "media_server/server_connector.h"
#include "utils/network/simple_http_client.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/network/module_finder.h"
#include "utils/network/direct_module_finder.h"
#include <utils/common/app_info.h>

namespace {
    ec2::AbstractECConnectionPtr ec2Connection() { return QnAppServerConnectionFactory::getConnection2(); }
}

int QnJoinSystemRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    Q_UNUSED(path)

    QUrl url = params.value(lit("url"));
    QString password = params.value(lit("password"));

    if (!url.isValid()) {
        result.setError(QnJsonRestResult::InvalidParameter);
        return CODE_INVALID_PARAMETER;
    }

    if (password.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter);
        return CODE_INVALID_PARAMETER;
    }

    /* Get module information to get system name. */

    QAuthenticator auth;
    auth.setUser(lit("admin"));
    auth.setPassword(password);

    CLSimpleHTTPClient client(url, 10000, auth);
    CLHttpStatus status = client.doGET(lit("api/moduleInformationAuthenticated"));

    if (status != CL_HTTP_SUCCESS) {
        if (status == CL_HTTP_AUTH_REQUIRED)
            result.setError(QnJsonRestResult::CantProcessRequest, lit("UNAUTHORIZED"));
        else
            result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
        return CODE_OK;
    }

    /* if we get it successfully we know system name and admin password */
    QByteArray data;
    client.readAll(data);

    QnJsonRestResult json;
    QJson::deserialize(data, &json);
    QnModuleInformation moduleInformation;
    QJson::deserialize(json.reply(), &moduleInformation);

    if (moduleInformation.systemName.isEmpty()) {
        /* Hmm there's no system name. It would be wrong system. Reject it. */
        result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
        return CODE_OK;
    }

    if (moduleInformation.version != qnCommon->engineVersion() || moduleInformation.customization != QnAppInfo::customizationName()) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE"));
        return CODE_OK;
    }

    if (!changeSystemName(moduleInformation.systemName)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("BACKUP_ERROR"));
        return CODE_OK;
    }
    changeAdminPassword(password);

    if (qnResPool->getResourceById(moduleInformation.id).isNull()) {
        if (!moduleInformation.remoteAddresses.contains(url.host())) {
            QUrl simpleUrl = url;
            url.setPath(QString());
            ec2Connection()->getDiscoveryManager()->addDiscoveryInformation(moduleInformation.id, QList<QUrl>() << simpleUrl, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        }
        QnModuleFinder::instance()->directModuleFinder()->checkUrl(url);
    }

    return CODE_OK;
}

bool QnJoinSystemRestHandler::changeSystemName(const QString &systemName) {
    QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();

    if (!backupDatabase())
        return false;

    if (systemName != qnCommon->localSystemName()) {
        MSSettings::roSettings()->setValue("systemName", systemName);
        qnCommon->setLocalSystemName(systemName);

        server->setSystemName(systemName);
        ec2Connection()->getMediaServerManager()->save(server, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }

    if (QnServerConnector::instance())
        QnServerConnector::instance()->reconnect();

    return true;
}

bool QnJoinSystemRestHandler::changeAdminPassword(const QString &password) {
    foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(Qn::user)) {
        QnUserResourcePtr user = resource.staticCast<QnUserResource>();
        if (user->getName() == lit("admin")) {
            user->setPassword(password);
            ec2Connection()->getUserManager()->save(user, this, [](int, ec2::ErrorCode) { return; });
            user->setPassword(QString());
            return true;
        }
    }
    return false;
}
