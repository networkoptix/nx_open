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
#include "network/tcp_connection_priv.h"
#include "network/module_finder.h"
#include "network/direct_module_finder.h"
#include "utils/common/app_info.h"
#include "nx/fusion/model_functions.h"
#include <nx/utils/log/log.h>
#include "api/model/ping_reply.h"
#include "audit/audit_manager.h"
#include "rest/server/rest_connection_processor.h"
#include "http/custom_headers.h"

#include <rest/helpers/permissions_helper.h>
#include <network/authenticate_helper.h>
#include <api/resource_property_adaptor.h>
#include <api/global_settings.h>
#include <nx/network/http/httpclient.h>

namespace
{
    const int requestTimeout = 60000;
    ec2::AbstractECConnectionPtr ec2Connection() { return QnAppServerConnectionFactory::getConnection2(); }

    bool isResponseOK(const nx_http::HttpClient& client)
    {
        if (!client.response())
            return false;
        return client.response()->statusLine.statusCode == nx_http::StatusCode::ok;
    }

    nx_http::StatusCode::Value getClientResponse(const nx_http::HttpClient& client)
    {
        if (client.response())
            return (nx_http::StatusCode::Value) client.response()->statusLine.statusCode;
        else
            return nx_http::StatusCode::undefined;
    }
}

int QnMergeSystemsRestHandler::executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor *owner)
{
    Q_UNUSED(path)
    if (QnPermissionsHelper::isSafeMode())
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->authUserId()))
        return QnPermissionsHelper::notOwnerError(result);

    QUrl url = params.value(lit("url"));
    QString remoteAdminPassword = params.value(lit("password"));
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

    if (remoteAdminPassword.isEmpty()) {
        NX_LOG(lit("QnMergeSystemsRestHandler. Request missing required parameter \"password\""), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("password")));
        return nx_http::StatusCode::ok;
    }

    /* Get module information to get system name. */

    QnUserResourcePtr adminUser = qnResPool->getAdministrator();
    if (!adminUser)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Failed to find admin user"), cl_logDEBUG1);
        return nx_http::StatusCode::internalServerError;
    }

    QByteArray moduleInformationData;
    {
        nx_http::HttpClient client;
        client.setResponseReadTimeoutMs(requestTimeout);
        client.setSendTimeoutMs(requestTimeout);
        client.setMessageBodyReadTimeoutMs(requestTimeout);

        QUrl requestUrl(url);
        requestUrl.setUserName(adminUser->getName());
        requestUrl.setPassword(remoteAdminPassword);
        requestUrl.setPath(lit("/api/moduleInformationAuthenticated"));
        requestUrl.setQuery(lit("showAddresses=true"));

        if (!client.doGet(requestUrl) || !isResponseOK(client))
        {
            auto status = getClientResponse(client);
            NX_LOG(lit("QnMergeSystemsRestHandler. Error requesting url %1: %2")
                .arg(url.toString()).arg(QLatin1String(nx_http::StatusCode::toString(status))),
                cl_logDEBUG1);
            if (status == nx_http::StatusCode::unauthorized)
                result.setError(QnJsonRestResult::CantProcessRequest, lit("UNAUTHORIZED"));
            else
                result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
            return nx_http::StatusCode::ok;
        }
        /* if we've got it successfully we know system name and admin password */
        moduleInformationData = client.fetchMessageBodyBuffer();
    }

    QnJsonRestResult json = QJson::deserialized<QnJsonRestResult>(moduleInformationData);
    QnModuleInformationWithAddresses remoteModuleInformation = json.deserialized<QnModuleInformationWithAddresses>();

    if (remoteModuleInformation.systemName.isEmpty())
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Remote (%1) system name is empty")
            .arg(url.toString()), cl_logDEBUG1);
        /* Hmm there's no system name. It would be wrong system. Reject it. */
        result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
        return nx_http::StatusCode::ok;
    }

    if (takeRemoteSettings && !qnCommon->moduleInformation().cloudSystemId.isEmpty())
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings is forbidden because system owner is cloud user")
            .arg(takeRemoteSettings), cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, lit("NOT_LOCAL_OWNER"));
        return nx_http::StatusCode::ok;
    }

    const bool canMerge =
        (remoteModuleInformation.cloudSystemId == qnCommon->moduleInformation().cloudSystemId) ||
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

    if ((!ignoreIncompatible && !compatible) || !customizationOK)
    {
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
        if (!backupDatabase())
        {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to backup database")
                .arg(takeRemoteSettings), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("BACKUP_ERROR"));
            return nx_http::StatusCode::ok;
        }

        if (!applyRemoteSettings(url, remoteModuleInformation.systemName, remoteAdminPassword, adminUser, owner))
        {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to apply remote settings")
                .arg(takeRemoteSettings), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("CONFIGURATION_ERROR"));
            return nx_http::StatusCode::ok;
        }
    } else
    {
        if (!backupDatabase())
        {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to backup database")
                .arg(takeRemoteSettings), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("BACKUP_ERROR"));
            return nx_http::StatusCode::ok;
        }

        if (!applyCurrentSettings(url, remoteAdminPassword, adminUser, mergeOneServer, owner))
        {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to apply current settings")
                .arg(takeRemoteSettings), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("CONFIGURATION_ERROR"));
            return nx_http::StatusCode::ok;
        }
    }

    /* Save additional address if needed */
    if (!remoteModuleInformation.remoteAddresses.contains(url.host()))
    {
        QUrl simpleUrl;
        simpleUrl.setScheme(lit("http"));
        simpleUrl.setHost(url.host());
        if (url.port() != remoteModuleInformation.port)
            simpleUrl.setPort(url.port());
        ec2Connection()->getDiscoveryManager(Qn::UserAccessData(owner->authUserId()))->addDiscoveryInformation(remoteModuleInformation.id,
                                                                                                               simpleUrl, false,
                                                                                                               ec2::DummyHandler::instance(),
                                                                                                               &ec2::DummyHandler::onRequestDone);
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
        const QString &remoteAdminPassword,
        const QnUserResourcePtr &admin,
        bool oneServer,
        const QnRestConnectionProcessor *owner)
{
    auto server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (!server)
        return false;

    ConfigureSystemData data;
    data.systemName = qnCommon->localSystemName();
    data.sysIdTime = qnCommon->systemIdentityTime();
    ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();
    data.tranLogTime = ec2Connection->getTransactionLogTime();
    data.wholeSystem = !oneServer;

    /**
     * Save current server to the foreign system.
     * It could be only way to pass authentication if current admin user is disabled
     */
    fromResourceToApi(server, data.foreignServer);

    /**
    * Save current user to the foreign system
    */
    fromResourceToApi(admin, data.foreignUser);

    /**
     * Save current system settings to the foreign system.
     */
    const auto& settings = QnGlobalSettings::instance()->allSettings();
    for (QnAbstractResourcePropertyAdaptor* setting : settings)
    {
        ec2::ApiResourceParamData param(setting->key(), setting->serializedValue());
        data.foreignSettings.push_back(param);
    }

    QByteArray serializedData = QJson::serialized(data);

    nx_http::HttpClient client;
    client.setResponseReadTimeoutMs(requestTimeout);
    client.setSendTimeoutMs(requestTimeout);
    client.setMessageBodyReadTimeoutMs(requestTimeout);
    client.addAdditionalHeader(Qn::AUTH_SESSION_HEADER_NAME, owner->authSession().toByteArray());

    QUrl requestUrl(remoteUrl);
    client.setUserName(admin->getName());
    client.setUserPassword(remoteAdminPassword);
    requestUrl.setPath(lit("/api/configure"));

    if (!client.doPost(requestUrl, "application/json", serializedData) ||
        !isResponseOK(client))
    {
        return false;
    }

    return true;
}

bool QnMergeSystemsRestHandler::applyRemoteSettings(
        const QUrl &remoteUrl,
        const QString &systemName,
        const QString &remoteAdminPassword,
        QnUserResourcePtr &admin,
        const QnRestConnectionProcessor *owner)
{
    qint64 remoteSysTime = 0;
    qint64 remoteTranLogTime = 0;

    {
        /* Read admin user from the remote server */

        nx_http::HttpClient client;
        client.setResponseReadTimeoutMs(requestTimeout);
        client.setSendTimeoutMs(requestTimeout);
        client.setMessageBodyReadTimeoutMs(requestTimeout);
        client.addAdditionalHeader(Qn::AUTH_SESSION_HEADER_NAME, owner->authSession().toByteArray());

        ec2::ApiUserDataList users;
        {
            QUrl requestUrl(remoteUrl);
            requestUrl.setUserName(admin->getName());
            requestUrl.setPassword(remoteAdminPassword);
            requestUrl.setPath(lit("/ec2/getUsers"));
            if (!client.doGet(requestUrl) || !isResponseOK(client))
            {
                auto status = getClientResponse(client);
                NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to invoke /ec2/getUsers: %1")
                    .arg(QLatin1String(nx_http::StatusCode::toString(status))), cl_logDEBUG1);
                return false;
            }

            QByteArray usersResponse = client.fetchMessageBodyBuffer();
            QJson::deserialize(usersResponse, &users);
        }

        {
            // ping remote system to take sysid time

            nx_http::HttpClient client;
            client.setResponseReadTimeoutMs(requestTimeout);
            client.setSendTimeoutMs(requestTimeout);
            client.setMessageBodyReadTimeoutMs(requestTimeout);
            client.addAdditionalHeader(Qn::AUTH_SESSION_HEADER_NAME, owner->authSession().toByteArray());

            QUrl requestUrl(remoteUrl);
            requestUrl.setUserName(admin->getName());
            requestUrl.setPassword(remoteAdminPassword);
            requestUrl.setPath(lit("/api/ping"));

            if (!client.doGet(requestUrl) || !isResponseOK(client))
            {
                auto status = getClientResponse(client);
                NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to invoke /api/ping: %1")
                    .arg(QLatin1String(nx_http::StatusCode::toString(status))), cl_logDEBUG1);
                return false;
            }

            QByteArray pingResponse = client.fetchMessageBodyBuffer();

            QnJsonRestResult result;
            QnPingReply reply;
            if (QJson::deserialize(pingResponse, &result) && QJson::deserialize(result.reply, &reply))
            {
                remoteSysTime = reply.sysIdTime;
                remoteTranLogTime = reply.tranLogTime;
            }
        }

        {
            // Backup database
            nx_http::HttpClient client;
            client.setResponseReadTimeoutMs(requestTimeout);
            client.setSendTimeoutMs(requestTimeout);
            client.setMessageBodyReadTimeoutMs(requestTimeout);
            client.addAdditionalHeader(Qn::AUTH_SESSION_HEADER_NAME, owner->authSession().toByteArray());

            QUrl requestUrl(remoteUrl);
            requestUrl.setUserName(admin->getName());
            requestUrl.setPassword(remoteAdminPassword);
            requestUrl.setPath(lit("/api/backupDatabase"));

            if (!client.doGet(requestUrl) || !isResponseOK(client))
            {
                auto status = getClientResponse(client);
                NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to invoke /api/backupDatabase: %1")
                    .arg(QLatin1String(nx_http::StatusCode::toString(status))), cl_logDEBUG1);
                return false;
            }
        }

        QnUserResourcePtr userResource;
        for (const ec2::ApiUserData &userData: users)
        {
            if (userData.id == admin->getId())
            {
                userResource = ec2::fromApiToResource(userData);
                break;
            }
        }
        if (userResource.isNull())
            return false;

        admin->update(userResource);
    }

    ec2::ErrorCode errorCode;

    ec2::ApiUserData userData;
    fromResourceToApi(admin, userData);
    errorCode = ec2Connection()->getUserManager(Qn::UserAccessData(owner->authUserId()))->saveSync(userData);
    NX_ASSERT(errorCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to save admin user: %1")
            .arg(ec2::toString(errorCode)), cl_logDEBUG1);
        return false;
    }

    ConfigureSystemData data;
    data.systemName = systemName;
    data.sysIdTime = remoteSysTime;
    data.tranLogTime = remoteTranLogTime;
    data.wholeSystem = true;

    if (!changeSystemName(data, Qn::UserAccessData(owner->authUserId())))
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to change system name"), cl_logDEBUG1);
        return false;
    }

    errorCode = ec2Connection()->getMiscManager(Qn::UserAccessData(owner->authUserId()))->changeSystemNameSync(systemName, remoteSysTime, remoteTranLogTime);
    NX_ASSERT(errorCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to save new system name: %1")
            .arg(ec2::toString(errorCode)), cl_logDEBUG1);
        return false;
    }

    return true;
}
