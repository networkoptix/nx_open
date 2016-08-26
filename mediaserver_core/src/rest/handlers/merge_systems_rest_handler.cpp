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
#include "system_settings_handler.h"
#include <rest/server/json_rest_result.h>
#include <api/model/system_settings_reply.h>
#include "api/model/password_data.h"
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

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

    void addAuthToRequest(QUrl& request, const QString& remoteAuthKey)
    {
        QUrlQuery query(request.query());
        query.addQueryItem(QLatin1String(Qn::URL_QUERY_AUTH_KEY_NAME), remoteAuthKey);
        request.setQuery(query);
    }
}

struct MergeSystemData
{
    MergeSystemData():
        takeRemoteSettings(false),
        mergeOneServer(false),
        ignoreIncompatible(false)
    {
    }

    MergeSystemData(const QnRequestParams& params):
        url(params.value(lit("url"))),
        getKey(params.value(lit("getKey"))),
        postKey(params.value(lit("postKey"))),
        takeRemoteSettings(params.value(lit("takeRemoteSettings"), lit("false")) != lit("false")),
        mergeOneServer(params.value(lit("oneServer"), lit("false")) != lit("false")),
        ignoreIncompatible(params.value(lit("ignoreIncompatible"), lit("false")) != lit("false"))
    {
    }

    QString url;
    QString getKey;
    QString postKey;
    bool takeRemoteSettings;
    bool mergeOneServer;
    bool ignoreIncompatible;
};

#define MergeSystemData_Fields (url)(getKey)(postKey)(takeRemoteSettings)(mergeOneServer)(ignoreIncompatible)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (MergeSystemData),
    (json),
    _Fields)



int QnMergeSystemsRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    return execute(std::move(MergeSystemData(params)), owner, result);
}

int QnMergeSystemsRestHandler::executePost(
    const QString& path,
    const QnRequestParams& params,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    Q_UNUSED(params);
    MergeSystemData data = QJson::deserialized<MergeSystemData>(body);
    return execute(std::move(data), owner, result);
}

int QnMergeSystemsRestHandler::execute(
    MergeSystemData data,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult &result)
{
    if (QnPermissionsHelper::isSafeMode())
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->accessRights()))
        return QnPermissionsHelper::notOwnerError(result);


    if (data.mergeOneServer)
        data.takeRemoteSettings = false;

    if (data.url.isEmpty())
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Request missing required parameter \"url\""), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("url")));
        return nx_http::StatusCode::ok;
    }

    QUrl url(data.url);
    if (!url.isValid())
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Received invalid parameter url %1")
            .arg(data.url), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::InvalidParameter, lit("url")));
        return nx_http::StatusCode::ok;
    }

    if (data.getKey.isEmpty())
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Request missing required parameter \"getKey\""), cl_logDEBUG1);
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

        QUrlQuery query;
        query.addQueryItem("checkOwnerPermissions", lit("true"));
        query.addQueryItem("showAddresses", lit("true"));

        QUrl requestUrl(url);
        requestUrl.setPath(lit("/api/moduleInformationAuthenticated"));
        requestUrl.setQuery(query);
        addAuthToRequest(requestUrl, data.getKey);

        if (!client.doGet(requestUrl) || !isResponseOK(client))
        {
            auto status = getClientResponse(client);
            NX_LOG(lit("QnMergeSystemsRestHandler. Error requesting url %1: %2")
                .arg(data.url).arg(QLatin1String(nx_http::StatusCode::toString(status))),
                cl_logDEBUG1);
            if (status == nx_http::StatusCode::unauthorized)
                result.setError(QnJsonRestResult::CantProcessRequest, lit("UNAUTHORIZED"));
            else if (status == nx_http::StatusCode::unauthorized)
                result.setError(QnJsonRestResult::CantProcessRequest, lit("FORBIDDEN"));
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
            .arg(data.url), cl_logDEBUG1);
        /* Hmm there's no system name. It would be wrong system. Reject it. */
        result.setError(QnJsonRestResult::CantProcessRequest, lit("FAIL"));
        return nx_http::StatusCode::ok;
    }

    bool isLocalInCloud = !qnCommon->moduleInformation().cloudSystemId.isEmpty();
    bool isRemoteInCloud = !remoteModuleInformation.cloudSystemId.isEmpty();
    if (isLocalInCloud && isRemoteInCloud)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler (%1). Cannot merge because both systems are bound to the cloud")
            .arg(data.url), cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, lit("BOTH_SYSTEM_BOUND_TO_CLOUD"));
        return nx_http::StatusCode::ok;
    }

    bool canMerge = true;
    if (remoteModuleInformation.cloudSystemId != qnCommon->moduleInformation().cloudSystemId)
    {
        if (data.takeRemoteSettings && isLocalInCloud)
            canMerge = false;
        else if (!data.takeRemoteSettings && isRemoteInCloud)
            canMerge = false;
    }

    if (!canMerge)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler (%1). Cannot merge systems bound to cloud")
            .arg(data.url), cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, lit("DEPENDENT_SYSTEM_BOUND_TO_CLOUD"));
        return nx_http::StatusCode::ok;
    }

    //TODO: #GDM #isCompatibleCustomization VMS-2163
    bool customizationOK = remoteModuleInformation.customization == QnAppInfo::customizationName() ||
                           remoteModuleInformation.customization.isEmpty() ||
                           QnModuleFinder::instance()->isCompatibilityMode();
    bool compatible = remoteModuleInformation.hasCompatibleVersion();

    if ((!data.ignoreIncompatible && !compatible) || !customizationOK)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Incompatible systems. Local customization %1, remote customization %2")
            .arg(QnAppInfo::customizationName()).arg(remoteModuleInformation.customization),
            cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, lit("INCOMPATIBLE"));
        return nx_http::StatusCode::ok;
    }

    QnMediaServerResourcePtr mServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    bool isDefaultSystemName;
    if (data.takeRemoteSettings)
        isDefaultSystemName = remoteModuleInformation.serverFlags.testFlag(Qn::SF_NewSystem);
    else
        isDefaultSystemName = mServer && (mServer->getServerFlags().testFlag(Qn::SF_NewSystem));
    if (isDefaultSystemName)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Can not merge to the non configured system"), cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, lit("UNCONFIGURED_SYSTEM"));
        return nx_http::StatusCode::ok;
    }

    if (!backupDatabase())
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to backup database")
            .arg(data.takeRemoteSettings), cl_logDEBUG1);
        result.setError(QnJsonRestResult::CantProcessRequest, lit("BACKUP_ERROR"));
        return nx_http::StatusCode::ok;
    }

    if (data.takeRemoteSettings)
    {
        if (!applyRemoteSettings(data.url, remoteModuleInformation.systemName, data.getKey, data.postKey, owner))
        {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to apply remote settings")
                .arg(data.takeRemoteSettings), cl_logDEBUG1);
            result.setError(QnJsonRestResult::CantProcessRequest, lit("CONFIGURATION_ERROR"));
            return nx_http::StatusCode::ok;
        }
    }
    else
    {
        if (!applyCurrentSettings(data.url, data.getKey, data.postKey, data.mergeOneServer, owner))
        {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to apply current settings")
                .arg(data.takeRemoteSettings), cl_logDEBUG1);
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
        auto discoveryManager = ec2Connection()->getDiscoveryManager(owner->accessRights());
        discoveryManager->addDiscoveryInformation(
            remoteModuleInformation.id,
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
    const QString& getKey,
    const QString& postKey,
    bool oneServer,
    const QnRestConnectionProcessor* owner)
{
    auto server = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (!server)
        return false;
    QnUserResourcePtr admin = qnResPool->getAdministrator();
    if (!admin)
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
    requestUrl.setPath(lit("/api/configure"));
    addAuthToRequest(requestUrl, postKey);
    if (!client.doPost(requestUrl, "application/json", serializedData) ||
        !isResponseOK(client))
    {
        return false;
    }

    return true;
}

template <class ResultDataType>
bool executeRequest(
    const QUrl &remoteUrl,
    const QString& getKey,
    ResultDataType& result,
    const QString& path)
{
    nx_http::HttpClient client;
    client.setResponseReadTimeoutMs(requestTimeout);
    client.setSendTimeoutMs(requestTimeout);
    client.setMessageBodyReadTimeoutMs(requestTimeout);


    QUrl requestUrl(remoteUrl);
    requestUrl.setPath(path);
    addAuthToRequest(requestUrl, getKey);
    if (!client.doGet(requestUrl) || !isResponseOK(client))
    {
        auto status = getClientResponse(client);
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to invoke %1: %2")
            .arg(path)
            .arg(QLatin1String(nx_http::StatusCode::toString(status))), cl_logDEBUG1);
        return false;
    }

    QByteArray response = client.fetchMessageBodyBuffer();
    return QJson::deserialize(response, &result);
}

bool QnMergeSystemsRestHandler::applyRemoteSettings(
    const QUrl& remoteUrl,
    const QString& systemName,
    const QString& getKey,
    const QString& postKey,
    const QnRestConnectionProcessor* owner)
{

    /* Read admin user from the remote server */

    ec2::ApiUserDataList users;
    if (!executeRequest(remoteUrl, getKey, users, lit("/ec2/getUsers")))
        return false;

    ec2::ApiMediaServerDataList servers;
    if (!executeRequest(remoteUrl, getKey, servers, lit("/ec2/getMediaServers")))
        return false;

    QnJsonRestResult settingsRestResult;
    if (!executeRequest(remoteUrl, getKey, settingsRestResult, lit("/api/systemSettings")))
        return false;

    QnSystemSettingsReply settings;
    if (!QJson::deserialize(settingsRestResult.reply, &settings))
        return false;

    QnJsonRestResult pingRestResult;
    if (!executeRequest(remoteUrl, getKey, pingRestResult, lit("/api/ping")))
        return false;

    QnPingReply pingReply;
    if (!QJson::deserialize(pingRestResult.reply, &pingReply))
        return false;

    QnJsonRestResult backupDBRestResult;
    if (!executeRequest(remoteUrl, getKey, backupDBRestResult, lit("/api/backupDatabase")))
        return false;

    ec2::ApiUserData adminUserData;
    QnUserResourcePtr admin = qnResPool->getAdministrator();
    if (!admin)
        return false;
    for (const auto& user: users)
    {
        if (user.id == admin->getId())
        {
            adminUserData = user;
            break;
        }
    }
    if (adminUserData.id.isNull())
        return false; //< not admin user in remote data

    ec2::ApiMediaServerData mediaServerData;
    for (const auto& server: servers)
    {
        if (server.id == pingReply.moduleGuid)
        {
            mediaServerData = server;
            break;
        }
    }
    if (mediaServerData.id.isNull())
        return false; //< no own media server in remote data

    auto userManager = ec2Connection()->getUserManager(owner->accessRights());
    ec2::ErrorCode errorCode = userManager->saveSync(adminUserData);
    NX_ASSERT(errorCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to save admin user: %1")
            .arg(ec2::toString(errorCode)), cl_logDEBUG1);
        return false;
    }

    auto serverManager = ec2Connection()->getMediaServerManager(owner->accessRights());
    errorCode = serverManager->saveSync(mediaServerData);
    NX_ASSERT(errorCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to save media server: %1")
            .arg(ec2::toString(errorCode)), cl_logDEBUG1);
        return false;
    }

    QnSystemSettingsHandler settingsSubHandler;
    const QnRequestParams settingsParams;
    QnJsonRestResult restSubResult;
    settingsSubHandler.executeGet(
        QString() /*path*/,
        settings.settings,
        restSubResult,
        owner);


    ConfigureSystemData data;
    data.systemName = systemName;
    data.sysIdTime = pingReply.sysIdTime;
    data.tranLogTime = pingReply.tranLogTime;
    data.wholeSystem = true;

    if (!changeSystemName(data))
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to change system name"), cl_logDEBUG1);
        return false;
    }

    auto miscManager = ec2Connection()->getMiscManager(owner->accessRights());
    errorCode = miscManager->changeSystemNameSync(systemName, pingReply.sysIdTime, pingReply.tranLogTime);
    NX_ASSERT(errorCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to save new system name: %1")
            .arg(ec2::toString(errorCode)), cl_logDEBUG1);
        return false;
    }

    return true;
}
