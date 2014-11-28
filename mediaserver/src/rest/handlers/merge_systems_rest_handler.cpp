#include "merge_systems_rest_handler.h"

#include <QtCore/QRegExp>

#include "core/resource_management/resource_pool.h"
#include "core/resource/user_resource.h"
#include "core/resource/media_server_resource.h"
#include "nx_ec/ec_api.h"
#include "nx_ec/dummy_handler.h"
#include "nx_ec/data/api_user_data.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "nx_ec/ec2_lib.h"
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
    QString user = params.value(lit("user"), lit("admin"));
    QString password = params.value(lit("password"));
    QString currentPassword = params.value(lit("currentPassword"));
    bool takeRemoteSettings = params.value(lit("takeRemoteSettings"), lit("false")) != lit("false");

    if (url.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter);
        result.setErrorString(lit("url"));
        return nx_http::StatusCode::ok;
    }

    if (!url.isValid()) {
        result.setError(QnJsonRestResult::InvalidParameter);
        result.setErrorString(lit("url"));
        return nx_http::StatusCode::ok;
    }

    if (password.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter);
        result.setErrorString(lit("password"));
        return nx_http::StatusCode::ok;
    }

    if (!takeRemoteSettings && currentPassword.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter);
        result.setErrorString(lit("currentPassword"));
        return nx_http::StatusCode::ok;
    }

    QnUserResourcePtr admin = qnResPool->getAdministrator();

    if (!admin)
        return nx_http::StatusCode::internalServerError;

    if (!admin->checkPassword(currentPassword)) {
        result.setError(QnJsonRestResult::InvalidParameter);
        result.setErrorString(lit("currentPassword"));
        return nx_http::StatusCode::ok;
    }

    /* Get module information to get system name. */

    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password);

    CLSimpleHTTPClient client(url, 10000, auth);
    CLHttpStatus status = client.doGET(lit("api/moduleInformationAuthenticated"));

    if (status != CL_HTTP_SUCCESS) {
        if (status == CL_HTTP_AUTH_REQUIRED)
            result.setError(QnJsonRestResult::CantProcessRequest, lit("UNAUTHORIZED"));
        else
            result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
        return nx_http::StatusCode::ok;
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
        return nx_http::StatusCode::ok;
    }

    bool customizationOK = moduleInformation.customization == QnAppInfo::customizationName() ||
                           moduleInformation.customization.isEmpty() ||
                           QnModuleFinder::instance()->isCompatibilityMode();
    if (!isCompatible(qnCommon->engineVersion(), moduleInformation.version) || !customizationOK) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE"));
        return nx_http::StatusCode::ok;
    }

    if (takeRemoteSettings) {
        if (!backupDatabase()) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("BACKUP_ERROR"));
            return nx_http::StatusCode::ok;
        }

        if (!applyRemoteSettings(url, moduleInformation.systemName, user, password, admin)) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("CONFIGURATION_ERROR"));
            return nx_http::StatusCode::ok;
        }

        return nx_http::StatusCode::ok;
    } else {
        if (!admin) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("INTERNAL_ERROR"));
            return nx_http::StatusCode::ok;
        }

        if (!applyCurrentSettings(url, user, password, currentPassword, admin)) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("CONFIGURATION_ERROR"));
            return nx_http::StatusCode::ok;
        }
    }

    /* Save additional address if needed */
    if (qnResPool->getResourceById(moduleInformation.id).isNull()) {
        if (!moduleInformation.remoteAddresses.contains(url.host())) {
            QUrl simpleUrl;
            simpleUrl.setScheme(lit("http"));
            simpleUrl.setHost(url.host());
            if (url.port() != moduleInformation.port)
                simpleUrl.setPort(url.port());
            ec2Connection()->getDiscoveryManager()->addDiscoveryInformation(moduleInformation.id, simpleUrl, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        }
        QnModuleFinder::instance()->directModuleFinder()->checkUrl(url);
    }

    if (QnServerConnector::instance())
        QnServerConnector::instance()->addConnection(moduleInformation, url);

    result.setReply(moduleInformation);

    return nx_http::StatusCode::ok;
}

bool QnMergeSystemsRestHandler::applyCurrentSettings(const QUrl &remoteUrl, const QString &user, const QString &password, const QString &currentPassword, const QnUserResourcePtr &admin) {
    QUrl ecUrl = remoteUrl;
    ecUrl.setUserName(user);

    {   /* Save current admin inside the remote system */
        ecUrl.setPassword(password);
        ec2::AbstractECConnectionPtr ec2Connection;
        ec2::ErrorCode errorCode = QnAppServerConnectionFactory::ec2ConnectionFactory()->connectSync(ecUrl, &ec2Connection);
        if (errorCode != ec2::ErrorCode::ok)
            return false;

        QnUserResourceList savedUsers;
        errorCode = ec2Connection->getUserManager()->saveSync(admin, &savedUsers);
        if (errorCode != ec2::ErrorCode::ok)
            return false;
    }

    {   /* Change system name of the remote system */
        ecUrl.setPassword(currentPassword);
        ec2::AbstractECConnectionPtr ec2Connection;
        ec2::ErrorCode errorCode = QnAppServerConnectionFactory::ec2ConnectionFactory()->connectSync(ecUrl, &ec2Connection);
        if (errorCode != ec2::ErrorCode::ok)
            return false;

        errorCode = ec2Connection->getMiscManager()->changeSystemNameSync(qnCommon->localSystemName());
        if (errorCode != ec2::ErrorCode::ok)
            return false;
    }

    return true;
}

bool QnMergeSystemsRestHandler::applyRemoteSettings(const QUrl &remoteUrl, const QString &systemName, const QString &user, const QString &password, QnUserResourcePtr &admin) {
    ec2::ErrorCode errorCode;

    QnUserResourceList users;
    {   /* Read admin user from the remote server */
        QUrl ecUrl = remoteUrl;
        ecUrl.setUserName(user);
        ecUrl.setPassword(password);

        ec2::AbstractECConnectionPtr ec2Connection;
        errorCode = QnAppServerConnectionFactory::ec2ConnectionFactory()->connectSync(ecUrl, &ec2Connection);
        if (errorCode != ec2::ErrorCode::ok)
            return false;

        errorCode = ec2Connection->getUserManager()->getUsersSync(admin->getId(), &users);
        if (errorCode != ec2::ErrorCode::ok || users.size() != 1)
            return false;
    }

    admin->update(users.first());
    errorCode = QnAppServerConnectionFactory::getConnection2()->getUserManager()->saveSync(users.first(), &users);
    if (errorCode != ec2::ErrorCode::ok)
        return false;

    if (!changeSystemName(systemName))
        return false;

    errorCode = QnAppServerConnectionFactory::getConnection2()->getMiscManager()->changeSystemNameSync(systemName);
    if (errorCode != ec2::ErrorCode::ok)
        return false;

    return true;
}
