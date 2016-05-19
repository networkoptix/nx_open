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
#include <nx_ec/managers/abstract_user_manager.h>

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
#include <nx/utils/log/log.h>
#include "api/model/ping_reply.h"
#include "audit/audit_manager.h"
#include "rest/server/rest_connection_processor.h"
#include "http/custom_headers.h"
#include "settings.h"


namespace
{
    const int requestTimeout = 60000;
    ec2::AbstractECConnectionPtr ec2Connection() { return QnAppServerConnectionFactory::getConnection2(); }
}

int QnMergeSystemsRestHandler::executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor *owner)
{
    Q_UNUSED(path)
    if (MSSettings::roSettings()->value(nx_ms_conf::EC_DB_READ_ONLY).toInt() ||
        ec2::Settings::instance()->dbReadOnly())
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Can't change parameters because server is running in safe mode"), cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Can't change parameters because server is running in safe mode"));
        return nx_http::StatusCode::forbidden;
    }

    QUrl url = params.value(lit("url"));
    QString password = params.value(lit("password"));
    QString currentPassword = params.value(lit("currentPassword"));
    bool takeRemoteSettings = params.value(lit("takeRemoteSettings"), lit("false")) != lit("false");
    bool mergeOneServer = params.value(lit("oneServer"), lit("false")) != lit("false");
    bool ignoreIncompatible = params.value(lit("ignoreIncompatible"), lit("false")) != lit("false");

    if (mergeOneServer)
        takeRemoteSettings = false;

    if (url.isEmpty()) {
        NX_LOG(lit("QnMergeSystemsRestHandler. Request missing required parameter \"url\""), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("url")));
        return nx_http::StatusCode::ok;
    }

    if (!url.isValid()) {
        NX_LOG(lit("QnMergeSystemsRestHandler. Received invalid parameter url %1")
            .arg(url.toString()), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::InvalidParameter, lit("url")));
        return nx_http::StatusCode::ok;
    }

    if (password.isEmpty()) {
        NX_LOG(lit("QnMergeSystemsRestHandler. Request missing required parameter \"password\""), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("password")));
        return nx_http::StatusCode::ok;
    }

    if (currentPassword.isEmpty()) {
        NX_LOG(lit("QnMergeSystemsRestHandler. Request missing required parameter \"currentPassword\""), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("currentPassword")));
        return nx_http::StatusCode::ok;
    }

    QnUserResourcePtr admin = qnResPool->getAdministrator();

    if (!admin)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Failed to find user \"admin\""), cl_logDEBUG1);
        return nx_http::StatusCode::internalServerError;
    }

    if (!admin->checkPassword(currentPassword)) {
        NX_LOG(lit("QnMergeSystemsRestHandler. Wrong admin password"), cl_logDEBUG1);
        result.setError(QnJsonRestResult::InvalidParameter, lit("currentPassword"));
        return nx_http::StatusCode::ok;
    }

    /* Get module information to get system name. */

    QAuthenticator auth;
    auth.setUser(admin->getName());
    auth.setPassword(password);

    CLSimpleHTTPClient client(url, 10000, auth);
    CLHttpStatus status = client.doGET(lit("api/moduleInformationAuthenticated?showAddresses=true"));

    if (status != CL_HTTP_SUCCESS) {
        NX_LOG(lit("QnMergeSystemsRestHandler. Error requesting url %1: %2")
            .arg(url.toString()).arg(QLatin1String(nx_http::StatusCode::toString(status))),
            cl_logDEBUG1);
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
    QnModuleInformationWithAddresses remoteModuleInformation = json.deserialized<QnModuleInformationWithAddresses>();

    if (remoteModuleInformation.systemName.isEmpty()) {
        NX_LOG(lit("QnMergeSystemsRestHandler. Remote (%1) system name is empty")
            .arg(url.toString()), cl_logDEBUG1);
        /* Hmm there's no system name. It would be wrong system. Reject it. */
        result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
        return nx_http::StatusCode::ok;
    }

    const bool canMerge = 
        (remoteModuleInformation.cloudSystemId == qnCommon->moduleInformation().cloudSystemId) ||
        (takeRemoteSettings && qnCommon->moduleInformation().cloudSystemId.isEmpty()) ||
        (!takeRemoteSettings && remoteModuleInformation.cloudSystemId.isEmpty());
    if (!canMerge)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler (%1). Cannot merge systems bound to cloud")
            .arg(url.toString()), cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, lit("DEPENDENT_SYSTEM_BOUND_TO_CLOUD"));
        return nx_http::StatusCode::ok;
    }

    bool customizationOK = remoteModuleInformation.customization == QnAppInfo::customizationName() ||
                           remoteModuleInformation.customization.isEmpty() ||
                           QnModuleFinder::instance()->isCompatibilityMode();
    bool compatible = remoteModuleInformation.hasCompatibleVersion();

    if ((!ignoreIncompatible && !compatible) || !customizationOK) {
        NX_LOG(lit("QnMergeSystemsRestHandler. Incompatible systems. Local customization %1, remote customization %2")
            .arg(QnAppInfo::customizationName()).arg(remoteModuleInformation.customization),
            cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE"));
        return nx_http::StatusCode::ok;
    }

    QnMediaServerResourcePtr mServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    bool isDefaultSystemName;
    if (takeRemoteSettings)
        isDefaultSystemName = remoteModuleInformation.serverFlags.testFlag(Qn::SF_NewSystem);
    else
        isDefaultSystemName = mServer && (mServer->getServerFlags().testFlag(Qn::SF_NewSystem));
    if (isDefaultSystemName)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Can not merge to the non configured system"), cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, lit("UNCONFIGURED_SYSTEM"));
        return nx_http::StatusCode::ok;
    }

    if (takeRemoteSettings)
    {
        if (!backupDatabase()) {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to backup database")
                .arg(takeRemoteSettings), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("BACKUP_ERROR"));
            return nx_http::StatusCode::ok;
        }

        if (!applyRemoteSettings(url, remoteModuleInformation.systemName, password, admin, owner)) {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to apply remote settings")
                .arg(takeRemoteSettings), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("CONFIGURATION_ERROR"));
            return nx_http::StatusCode::ok;
        }
    } else
    {
        if (!admin) {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. No admin")
                .arg(takeRemoteSettings), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("INTERNAL_ERROR"));
            return nx_http::StatusCode::ok;
        }

        if (!backupDatabase()) {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to backup database")
                .arg(takeRemoteSettings), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("BACKUP_ERROR"));
            return nx_http::StatusCode::ok;
        }

        if (!applyCurrentSettings(url, password, currentPassword, admin, mergeOneServer, owner)) {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to apply current settings")
                .arg(takeRemoteSettings), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("CONFIGURATION_ERROR"));
            return nx_http::StatusCode::ok;
        }
    }

    /* Save additional address if needed */
    if (!remoteModuleInformation.remoteAddresses.contains(url.host())) {
        QUrl simpleUrl;
        simpleUrl.setScheme(lit("http"));
        simpleUrl.setHost(url.host());
        if (url.port() != remoteModuleInformation.port)
            simpleUrl.setPort(url.port());
        ec2Connection()->getDiscoveryManager()->addDiscoveryInformation(remoteModuleInformation.id, simpleUrl, false, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }
    QnModuleFinder::instance()->directModuleFinder()->checkUrl(url);

    /* Connect to server if it is compatible */
    if (compatible && QnServerConnector::instance())
        QnServerConnector::instance()->addConnection(remoteModuleInformation, SocketAddress(url.host(), remoteModuleInformation.port));

    result.setReply(remoteModuleInformation);

    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(owner->authSession(), Qn::AR_SystemmMerge);
    qnAuditManager->addAuditRecord(auditRecord);


    return nx_http::StatusCode::ok;
}

bool QnMergeSystemsRestHandler::applyCurrentSettings(
        const QUrl &remoteUrl,
        const QString &password,
        const QString &currentPassword,
        const QnUserResourcePtr &admin,
        bool oneServer,
        const QnRestConnectionProcessor *owner)
{
    QAuthenticator authenticator;
    authenticator.setUser(admin->getName());
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

bool QnMergeSystemsRestHandler::applyRemoteSettings(
        const QUrl &remoteUrl,
        const QString &systemName,
        const QString &password,
        QnUserResourcePtr &admin,
        const QnRestConnectionProcessor *owner)
{
    qint64 remoteSysTime = 0;
    qint64 remoteTranLogTime = 0;

    {
        /* Read admin user from the remote server */
        QAuthenticator authenticator;
        authenticator.setUser(admin->getName());
        authenticator.setPassword(password);

        ec2::ApiUserDataList users;
        {
            CLSimpleHTTPClient client(remoteUrl, requestTimeout, authenticator);
            CLHttpStatus status = client.doGET(lit("/ec2/getUsers"));
            if (status != CLHttpStatus::CL_HTTP_SUCCESS)
            {
                NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to invoke /ec2/getUsers: %1")
                    .arg(QLatin1String(nx_http::StatusCode::toString(status))), cl_logDEBUG1);
                return false;
            }

            QByteArray data;
            client.readAll(data);

            QJson::deserialize(data, &users);
        }
        {
            CLSimpleHTTPClient client(remoteUrl, requestTimeout, authenticator);
            CLHttpStatus status = client.doGET(lit("/api/ping"));
            if (status != CLHttpStatus::CL_HTTP_SUCCESS)
            {
                NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to invoke /api/ping: %1")
                    .arg(QLatin1String(nx_http::StatusCode::toString(status))), cl_logDEBUG1);
                return false;
            }

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
            {
                NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to invoke /api/backupDatabase: %1")
                    .arg(QLatin1String(nx_http::StatusCode::toString(status))), cl_logDEBUG1);
                return false;
            }
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

    ec2::ApiUserData userData;
    fromResourceToApi(admin, userData);
    errorCode = ec2Connection()->getUserManager()->saveSync(userData);
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to save admin user: %1")
            .arg(ec2::toString(errorCode)), cl_logDEBUG1);
        return false;
    }

    if (!changeSystemName(systemName, remoteSysTime, remoteTranLogTime))
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to change system name"), cl_logDEBUG1);
        return false;
    }

    errorCode = ec2Connection()->getMiscManager()->changeSystemNameSync(systemName, remoteSysTime, remoteTranLogTime);
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to save new system name: %1")
            .arg(ec2::toString(errorCode)), cl_logDEBUG1);
        return false;
    }

    return true;
}
