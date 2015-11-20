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
#include <nx/network/simple_http_client.h>
#include "network/tcp_connection_priv.h"
#include "network/module_finder.h"
#include "network/direct_module_finder.h"
#include "utils/common/app_info.h"
#include "utils/common/model_functions.h"
#include "api/model/ping_reply.h"
#include "audit/audit_manager.h"
#include "rest/server/rest_connection_processor.h"
#include "http/custom_headers.h"
#include "settings.h"

namespace {
    const int requestTimeout = 60000;
    ec2::AbstractECConnectionPtr ec2Connection() { return QnAppServerConnectionFactory::getConnection2(); }
}

int QnMergeSystemsRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor* owner) 
{
    Q_UNUSED(path)
    if (MSSettings::roSettings()->value(nx_ms_conf::EC_DB_READ_ONLY).toInt()) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't change parameters because server is running in safe mode"));
        return nx_http::StatusCode::forbidden;
    }

    if (ec2::Settings::instance()->dbReadOnly()) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't change parameters because server is running in safe mode"));
        return nx_http::StatusCode::forbidden;
    }

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
        result.setError(QnJsonRestResult::MissingParameter, lit("url"));
        return nx_http::StatusCode::ok;
    }

    if (!url.isValid()) {
        result.setError(QnJsonRestResult::InvalidParameter, lit("url"));
        return nx_http::StatusCode::ok;
    }

    if (password.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter, lit("password"));
        return nx_http::StatusCode::ok;
    }

    if (currentPassword.isEmpty()) {
        result.setError(QnJsonRestResult::MissingParameter, lit("currentPassword"));
        return nx_http::StatusCode::ok;
    }

    QnUserResourcePtr admin = qnResPool->getAdministrator();

    if (!admin)
        return nx_http::StatusCode::internalServerError;

    if (!admin->checkPassword(currentPassword)) {
        result.setError(QnJsonRestResult::InvalidParameter, lit("currentPassword"));
        return nx_http::StatusCode::ok;
    }

    /* Get module information to get system name. */

    QAuthenticator auth;
    auth.setUser(user);
    auth.setPassword(password);

    CLSimpleHTTPClient client(url, 10000, auth);
    CLHttpStatus status = client.doGET(lit("api/moduleInformationAuthenticated?showAddresses=true"));

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

    QnJsonRestResult json = QJson::deserialized<QnJsonRestResult>(data);
    QnModuleInformationWithAddresses moduleInformation = json.deserialized<QnModuleInformationWithAddresses>();

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

        if (!applyRemoteSettings(url, moduleInformation.systemName, user, password, admin, owner)) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("CONFIGURATION_ERROR"));
            return nx_http::StatusCode::ok;
        }
    } else {
        if (!admin) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("INTERNAL_ERROR"));
            return nx_http::StatusCode::ok;
        }

        if (!backupDatabase()) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("BACKUP_ERROR"));
            return nx_http::StatusCode::ok;
        }

        if (!applyCurrentSettings(url, user, password, currentPassword, admin, mergeOneServer, owner)) {
            result.setError(QnJsonRestResult::CantProcessRequest, lit("CONFIGURATION_ERROR"));
            return nx_http::StatusCode::ok;
        }
    }

    /* Save additional address if needed */
    if (!moduleInformation.remoteAddresses.contains(url.host())) {
        QUrl simpleUrl;
        simpleUrl.setScheme(lit("http"));
        simpleUrl.setHost(url.host());
        if (url.port() != moduleInformation.port)
            simpleUrl.setPort(url.port());
        ec2Connection()->getDiscoveryManager()->addDiscoveryInformation(moduleInformation.id, simpleUrl, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }
    QnModuleFinder::instance()->directModuleFinder()->checkUrl(url);

    /* Connect to server if it is compatible */
    if (compatible && QnServerConnector::instance())
        QnServerConnector::instance()->addConnection(moduleInformation, SocketAddress(url.host(), moduleInformation.port));

    result.setReply(moduleInformation);

    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(owner->authSession(), Qn::AR_SystemmMerge);
    qnAuditManager->addAuditRecord(auditRecord);


    return nx_http::StatusCode::ok;
}

bool QnMergeSystemsRestHandler::applyCurrentSettings(const QUrl &remoteUrl, const QString &user, const QString &password, const QString &currentPassword, const QnUserResourcePtr &admin, bool oneServer,
                                                     const QnRestConnectionProcessor* owner)
{
    QAuthenticator authenticator;
    authenticator.setUser(user);
    authenticator.setPassword(password);

    QString systemName = QString::fromUtf8(QUrl::toPercentEncoding(qnCommon->localSystemName()));

    //saving user data before /api/configure call to prevent race condition, 
    //  when we establish transaction connection to remote server and receive updated 
    //  saveUser transaction before passing admin to /ec2/saveUser call
    {   /* Save current admin inside the remote system */

        /* Change system name of the selected server */
        CLSimpleHTTPClient client(remoteUrl, requestTimeout, authenticator);

        ec2::ApiUserData userData;
        ec2::fromResourceToApi(admin, userData);
        const QByteArray saveUserData = QJson::serialized(userData);
        client.addHeader("Content-Type", "application/json");
        CLHttpStatus status = client.doPOST(lit("/ec2/saveUser"), saveUserData);

        if (status != CLHttpStatus::CL_HTTP_SUCCESS)
            return false;
    }

    authenticator.setPassword(currentPassword);
    /* Change system name of the selected server */
    CLSimpleHTTPClient client(remoteUrl, requestTimeout, authenticator);

    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    client.addHeader(Qn::AUTH_SESSION_HEADER_NAME, owner->authSession().toByteArray());
    QString requestStr = lit("/api/configure?systemName=%1").arg(systemName);
    requestStr += lit("&sysIdTime=%1").arg(qnCommon->systemIdentityTime());
    requestStr += lit("&tranLogTime=%1").arg(ec2Connection->getTransactionLogTime());
    if (oneServer) {
        auto authSession = owner->authSession();
        CLHttpStatus status = client.doGET(requestStr);
        if (status != CLHttpStatus::CL_HTTP_SUCCESS)
            return false;
    }

    /* Change system name of the remote system */
    if (!oneServer) {
        authenticator.setPassword(currentPassword);
        CLSimpleHTTPClient client(remoteUrl, requestTimeout, authenticator);
        client.addHeader(Qn::AUTH_SESSION_HEADER_NAME, owner->authSession().toByteArray());
        CLHttpStatus status = client.doGET(requestStr + lit("&wholeSystem=true"));
        if (status != CLHttpStatus::CL_HTTP_SUCCESS)
            return false;
    }

    return true;
}

bool QnMergeSystemsRestHandler::applyRemoteSettings(const QUrl &remoteUrl, const QString &systemName, const QString &user, const QString &password, QnUserResourcePtr &admin,
                                                    const QnRestConnectionProcessor* owner) 
{
    qint64 remoteSysTime = 0;
    qint64 remoteTranLogTime = 0;
    {   /* Read admin user from the remote server */
        QAuthenticator authenticator;
        authenticator.setUser(user);
        authenticator.setPassword(password);

        ec2::ApiUserDataList users;
        {
            CLSimpleHTTPClient client(remoteUrl, requestTimeout, authenticator);
            CLHttpStatus status = client.doGET(lit("/ec2/getUsers"));
            if (status != CLHttpStatus::CL_HTTP_SUCCESS)
                return false;

            QByteArray data;
            client.readAll(data);

            QJson::deserialize(data, &users);
        }
        {
            CLSimpleHTTPClient client(remoteUrl, requestTimeout, authenticator);
            CLHttpStatus status = client.doGET(lit("/api/ping"));
            if (status != CLHttpStatus::CL_HTTP_SUCCESS)
                return false;

            QByteArray data;
            client.readAll(data);

            QnJsonRestResult result;
            QnPingReply reply;
            if (QJson::deserialize(data, &result) && QJson::deserialize(result.reply, &reply)) {
                remoteSysTime = reply.sysIdTime;
                remoteTranLogTime = reply.tranLogTime;
            }
        }
        {
            CLSimpleHTTPClient client(remoteUrl, requestTimeout, authenticator);
            client.addHeader(Qn::AUTH_SESSION_HEADER_NAME, owner->authSession().toByteArray());
            CLHttpStatus status = client.doGET(lit("/api/backupDatabase"));
            if (status != CLHttpStatus::CL_HTTP_SUCCESS)
                return false;
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

    if (!changeSystemName(systemName, remoteSysTime, remoteTranLogTime))
        return false;

    errorCode = ec2Connection()->getMiscManager()->changeSystemNameSync(systemName, remoteSysTime, remoteTranLogTime);
    if (errorCode != ec2::ErrorCode::ok)
        return false;

    return true;
}
