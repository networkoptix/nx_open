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
#include "utils/common/model_functions.h"
#include "api/model/ping_reply.h"

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
    bool mergeOneServer = params.value(lit("oneServer"), lit("false")) != lit("false");
    bool ignoreIncompatible = params.value(lit("ignoreIncompatible"), lit("false")) != lit("false");

    if (mergeOneServer)
        takeRemoteSettings = false;

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

    if (!takeRemoteSettings && !mergeOneServer && currentPassword.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter);
        result.setErrorString(lit("currentPassword"));
        return nx_http::StatusCode::ok;
    }

    QnUserResourcePtr admin = qnResPool->getAdministrator();

    if (!admin)
        return nx_http::StatusCode::internalServerError;

    if (!takeRemoteSettings && !mergeOneServer && !admin->checkPassword(currentPassword)) {
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
    bool compatible = moduleInformation.hasCompatibleVersion();

    if ((!ignoreIncompatible && !compatible) || !customizationOK) {
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
    } else {
        if (!admin) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("INTERNAL_ERROR"));
            return nx_http::StatusCode::ok;
        }

        if (!applyCurrentSettings(url, user, password, currentPassword, admin, mergeOneServer)) {
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

    /* Connect to server if it is compatible */
    if (compatible && QnServerConnector::instance())
        QnServerConnector::instance()->addConnection(moduleInformation, url);

    result.setReply(moduleInformation);

    return nx_http::StatusCode::ok;
}

bool QnMergeSystemsRestHandler::applyCurrentSettings(const QUrl &remoteUrl, const QString &user, const QString &password, const QString &currentPassword, const QnUserResourcePtr &admin, bool oneServer) 
{
    QAuthenticator authenticator;
    authenticator.setUser(user);
    authenticator.setPassword(password);

    /* Change system name of the selected server */
    if (oneServer) {
        CLSimpleHTTPClient client(remoteUrl, 10000, authenticator);
        CLHttpStatus status = client.doGET(lit("/api/configure?systemName=%1&sysIdTime=%2")
            .arg(qnCommon->localSystemName())
            .arg(qnCommon->systemIdentityTime()));
        if (status != CLHttpStatus::CL_HTTP_SUCCESS)
            return false;
    }

    {   /* Save current admin inside the remote system */
        CLSimpleHTTPClient client(remoteUrl, 10000, authenticator);

        ec2::ApiUserData userData;
        ec2::fromResourceToApi(admin, userData);
        QByteArray data = QJson::serialized(userData);

        client.addHeader("Content-Type", "application/json");
        CLHttpStatus status = client.doPOST(lit("/ec2/saveUser"), data);

        if (status != CLHttpStatus::CL_HTTP_SUCCESS)
            return false;
    }

    /* Change system name of the remote system */
    if (!oneServer) {
        authenticator.setPassword(currentPassword);
        CLSimpleHTTPClient client(remoteUrl, 10000, authenticator);
        CLHttpStatus status = client.doGET(lit("/api/configure?systemName=%1&wholeSystem=true&sysIdTime=%2")
            .arg(qnCommon->localSystemName())
            .arg(qnCommon->systemIdentityTime()));
        if (status != CLHttpStatus::CL_HTTP_SUCCESS)
            return false;
    }

    return true;
}

bool QnMergeSystemsRestHandler::applyRemoteSettings(const QUrl &remoteUrl, const QString &systemName, const QString &user, const QString &password, QnUserResourcePtr &admin) 
{
    qint64 remoteSysTime = 0;
    {   /* Read admin user from the remote server */
        QAuthenticator authenticator;
        authenticator.setUser(user);
        authenticator.setPassword(password);

        ec2::ApiUserDataList users;
        {
            CLSimpleHTTPClient client(remoteUrl, 10000, authenticator);
            CLHttpStatus status = client.doGET(lit("/ec2/getUsers"));
            if (status != CLHttpStatus::CL_HTTP_SUCCESS)
                return false;

            QByteArray data;
            client.readAll(data);

            QJson::deserialize(data, &users);
        }
        {
            CLSimpleHTTPClient client(remoteUrl, 10000, authenticator);
            CLHttpStatus status = client.doGET(lit("/api/ping"));
            if (status != CLHttpStatus::CL_HTTP_SUCCESS)
                return false;

            QByteArray data;
            client.readAll(data);

            QnJsonRestResult result;
            QnPingReply reply;
            if (QJson::deserialize(data, &result) && QJson::deserialize(result.reply(), &reply))
                remoteSysTime = reply.sysIdTime;
        }

        QnUserResourcePtr userResource = QnUserResourcePtr(new QnUserResource());
        for (const ec2::ApiUserData &userData: users) {
            if (userData.id == admin->getId()) {
                ec2::fromApiToResource(userData, userResource);
                break;
            }
        }
        if (userResource->getId() != admin->getId())
            return false;

        admin->update(userResource);
    }

    ec2::ErrorCode errorCode;
    QnUserResourceList users;

    errorCode = ec2Connection()->getUserManager()->saveSync(admin, &users);
    if (errorCode != ec2::ErrorCode::ok)
        return false;

    if (!changeSystemName(systemName, remoteSysTime))
        return false;

    errorCode = ec2Connection()->getMiscManager()->changeSystemNameSync(systemName, remoteSysTime);
    if (errorCode != ec2::ErrorCode::ok)
        return false;

    return true;
}
