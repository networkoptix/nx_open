#include "merge_systems_rest_handler.h"

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
#include "utils/common/app_info.h"

namespace {
    ec2::AbstractECConnectionPtr ec2Connection() { return QnAppServerConnectionFactory::getConnection2(); }
}

int QnMergeSystemsRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner) 
{
    Q_UNUSED(path)
    Q_UNUSED(owner)

    QUrl url = params.value(lit("url"));
    QString password = params.value(lit("password"));
    bool takeRemoteSettings = params.value(lit("takeRemoteSettings"), lit("false")) != lit("false");

    if (url.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter);
        result.setErrorString(lit("url"));
        return CODE_OK;
    }

    if (!url.isValid()) {
        result.setError(QnJsonRestResult::InvalidParameter);
        result.setErrorString(lit("url"));
        return CODE_OK;
    }

    if (password.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter);
        result.setErrorString(lit("password"));
        return CODE_OK;
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

    /* if we've got it successfully we know system name and admin password */
    QByteArray data;
    client.readAll(data);
    client.close();

    QnJsonRestResult json;
    QJson::deserialize(data, &json);
    QnModuleInformation moduleInformation;
    QJson::deserialize(json.reply(), &moduleInformation);

    if (moduleInformation.systemName.isEmpty()) {
        /* Hmm there's no system name. It would be wrong system. Reject it. */
        result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
        return CODE_OK;
    }

    bool customizationOK = moduleInformation.customization == QnAppInfo::customizationName();
    if (QnModuleFinder::instance()->isCompatibilityMode())
        customizationOK = true;
    if (!isCompatible(qnCommon->engineVersion(), moduleInformation.version) || !customizationOK) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE"));
        return CODE_OK;
    }

    if (takeRemoteSettings) {
        if (!changeSystemName(moduleInformation.systemName)) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("BACKUP_ERROR"));
            return CODE_OK;
        }
        changeAdminPassword(password);
    } else {
        QnUserResourcePtr admin = qnResPool->getAdministrator();
        if (!admin) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("INTERNAL_ERROR"));
            return CODE_OK;
        }

        QString request = lit("api/configure?systemName=%1&wholeSystem=true&passwordHash=%2&passwordDigest=%3")
                          .arg(qnCommon->localSystemName())
                          .arg(QString::fromLatin1(admin->getHash()))
                          .arg(QString::fromLatin1(admin->getDigest()));

        status = client.doGET(request);
        if (status != CL_HTTP_SUCCESS) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("CONFIGURATION_ERROR"));
            return CODE_OK;
        }
    }

    if (qnResPool->getResourceById(moduleInformation.id).isNull()) {
        if (!moduleInformation.remoteAddresses.contains(url.host())) {
            QUrl simpleUrl = url;
            url.setPath(QString());
            ec2Connection()->getDiscoveryManager()->addDiscoveryInformation(moduleInformation.id, simpleUrl, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        }
        QnModuleFinder::instance()->directModuleFinder()->checkUrl(url);
    }

    if (QnServerConnector::instance())
        QnServerConnector::instance()->addConnection(moduleInformation, url);

    result.setReply(moduleInformation);

    return CODE_OK;
}

bool QnMergeSystemsRestHandler::changeSystemName(const QString &systemName) {
    QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();

    if (!backupDatabase())
        return false;

    if (systemName == qnCommon->localSystemName())
        return true;

    MSSettings::roSettings()->setValue("systemName", systemName);
    qnCommon->setLocalSystemName(systemName);

    server->setSystemName(systemName);
    ec2Connection()->getMediaServerManager()->save(server, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    QnAppServerConnectionFactory::getConnection2()->getMiscManager()->changeSystemName(systemName, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    return true;
}

bool QnMergeSystemsRestHandler::changeAdminPassword(const QString &password) {
    if (QnUserResourcePtr admin = getAdminUser()) {
        admin->setPassword(password);
        admin->generateHash();
        ec2Connection()->getUserManager()->save(admin, this, [](int, ec2::ErrorCode) { return; });
        return true;
    }
    return false;
}
