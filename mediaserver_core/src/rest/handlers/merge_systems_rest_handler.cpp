#include "merge_systems_rest_handler.h"

#include <chrono>

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
#include "nx/vms/discovery/manager.h"
#include <network/connection_validator.h>

#include "utils/common/app_info.h"
#include "nx/fusion/model_functions.h"
#include <nx/utils/log/log.h>
#include "api/model/ping_reply.h"
#include "audit/audit_manager.h"
#include "rest/server/rest_connection_processor.h"
#include <nx/network/http/custom_headers.h>

#include <rest/helpers/permissions_helper.h>
#include <network/authenticate_helper.h>
#include <api/resource_property_adaptor.h>
#include <api/global_settings.h>
#include <nx/network/http/http_client.h>
#include "system_settings_handler.h"
#include <rest/server/json_rest_result.h>
#include <api/model/system_settings_reply.h>
#include "api/model/password_data.h"
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_management/resource_properties.h>

namespace {

    using namespace std::chrono;

    static const milliseconds kRequestTimeout = seconds(60);

    // Minimal server version which could be configured.
    static const QnSoftwareVersion kMinimalVersion(2, 3);

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

    void addAuthToRequest(nx::utils::Url& request, const QString& remoteAuthKey)
    {
        QUrlQuery query(request.query());
        query.addQueryItem(QLatin1String(Qn::URL_QUERY_AUTH_KEY_NAME), remoteAuthKey);
        request.setQuery(query);
    }

} // namespace

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    MergeSystemData, (json),
    (url)(getKey)(postKey)(takeRemoteSettings)(mergeOneServer)(ignoreIncompatible))

QnMergeSystemsRestHandler::QnMergeSystemsRestHandler(ec2::AbstractTransactionMessageBus* messageBus):
    QnJsonRestHandler(),
    m_messageBus(messageBus)
{}

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
    using MergeStatus = utils::MergeSystemsStatus::Value;

    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
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

    nx::utils::Url url(data.url);
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
    QnUserResourcePtr adminUser = owner->resourcePool()->getAdministrator();
    if (!adminUser)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Failed to find admin user"), cl_logDEBUG1);
        return nx_http::StatusCode::internalServerError;
    }

    QByteArray moduleInformationData;
    {
        nx_http::HttpClient client;
        client.setResponseReadTimeoutMs(kRequestTimeout.count());
        client.setSendTimeoutMs(kRequestTimeout.count());
        client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());

        QUrlQuery query;
        query.addQueryItem("checkOwnerPermissions", lit("true"));
        query.addQueryItem("showAddresses", lit("true"));

        nx::utils::Url requestUrl(url);
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
                setMergeError(result, MergeStatus::unauthorized);
            else if (status == nx_http::StatusCode::unauthorized)
                setMergeError(result, MergeStatus::forbidden);
            else
                setMergeError(result, MergeStatus::notFound);
            return nx_http::StatusCode::ok;
        }
        /* if we've got it successfully we know system name and admin password */
        while (!client.eof())
            moduleInformationData.append(client.fetchMessageBodyBuffer());
    }

    const auto json = QJson::deserialized<QnJsonRestResult>(moduleInformationData);
    const auto remoteModuleInformation = json.deserialized<QnModuleInformationWithAddresses>();

    result.setReply(remoteModuleInformation);

    if (remoteModuleInformation.version < kMinimalVersion)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Remote system has too old version %2 (%1)")
            .arg(data.url).arg(remoteModuleInformation.version.toString()), cl_logDEBUG1);
        setMergeError(result, MergeStatus::incompatibleVersion);
        return nx_http::StatusCode::ok;
    }

    bool isLocalInCloud = !owner->commonModule()->globalSettings()->cloudSystemId().isEmpty();
    bool isRemoteInCloud = !remoteModuleInformation.cloudSystemId.isEmpty();
    if (isLocalInCloud && isRemoteInCloud)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler (%1). Cannot merge because both systems are bound to the cloud")
            .arg(data.url), cl_logDEBUG1);
        setMergeError(result, MergeStatus::bothSystemBoundToCloud);
        return nx_http::StatusCode::ok;
    }

    bool canMerge = true;
    if (remoteModuleInformation.cloudSystemId != owner->commonModule()->globalSettings()->cloudSystemId())
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
        setMergeError(result, MergeStatus::dependentSystemBoundToCloud);
        return nx_http::StatusCode::ok;
    }

    auto connectionResult = QnConnectionValidator::validateConnection(remoteModuleInformation);
    if (connectionResult == Qn::IncompatibleInternalConnectionResult
        || connectionResult == Qn::IncompatibleCloudHostConnectionResult
        || connectionResult == Qn::IncompatibleVersionConnectionResult)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Incompatible systems. "
            "Local customization %1, cloud host %2, "
            "remote customization %3, cloud host %4, version %5")
            .arg(QnAppInfo::customizationName())
            .arg(nx::network::AppInfo::defaultCloudHost())
            .arg(remoteModuleInformation.customization)
            .arg(remoteModuleInformation.cloudHost)
            .arg(remoteModuleInformation.version.toString()),
            cl_logDEBUG1);
        setMergeError(result, MergeStatus::incompatibleVersion);
        return nx_http::StatusCode::ok;
    }

    if (connectionResult == Qn::IncompatibleProtocolConnectionResult
        && !data.ignoreIncompatible)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Incompatible systems protocol. "
                   "Local %1, remote %2")
            .arg(QnAppInfo::ec2ProtoVersion())
            .arg(remoteModuleInformation.protoVersion),
            cl_logDEBUG1);
        setMergeError(result, MergeStatus::incompatibleVersion);
        return nx_http::StatusCode::ok;
    }

    QnMediaServerResourcePtr mServer = owner->resourcePool()->getResourceById<QnMediaServerResource>(owner->commonModule()->moduleGUID());
    bool isDefaultSystemName;
    if (data.takeRemoteSettings)
        isDefaultSystemName = remoteModuleInformation.serverFlags.testFlag(Qn::SF_NewSystem);
    else
        isDefaultSystemName = mServer && (mServer->getServerFlags().testFlag(Qn::SF_NewSystem));
    if (isDefaultSystemName)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. Can not merge to the non configured system"), cl_logDEBUG1);
        setMergeError(result, MergeStatus::unconfiguredSystem);
        return nx_http::StatusCode::ok;
    }

    if (!backupDatabase(owner->commonModule()->ec2Connection()))
    {
        NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to backup database")
            .arg(data.takeRemoteSettings), cl_logDEBUG1);
        setMergeError(result, MergeStatus::backupFailed);
        return nx_http::StatusCode::ok;
    }

    if (data.takeRemoteSettings)
    {
        if (!applyRemoteSettings(data.url,
                remoteModuleInformation.localSystemId, remoteModuleInformation.systemName,
                data.getKey, data.postKey, owner))
        {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to apply remote settings")
                .arg(data.takeRemoteSettings), cl_logDEBUG1);
            setMergeError(result, MergeStatus::configurationFailed);
            return nx_http::StatusCode::ok;
        }
    }
    else
    {
        if (!applyCurrentSettings(data.url, data.getKey, data.postKey, data.mergeOneServer, owner))
        {
            NX_LOG(lit("QnMergeSystemsRestHandler. takeRemoteSettings %1. Failed to apply current settings")
                .arg(data.takeRemoteSettings), cl_logDEBUG1);
            setMergeError(result, MergeStatus::configurationFailed);
            return nx_http::StatusCode::ok;
        }
    }

    /* Save additional address if needed */
    if (!remoteModuleInformation.remoteAddresses.contains(url.host()))
    {
        nx::utils::Url simpleUrl;
        simpleUrl.setScheme(lit("http"));
        simpleUrl.setHost(url.host());
        if (url.port() != remoteModuleInformation.port)
            simpleUrl.setPort(url.port());
        auto discoveryManager = owner->commonModule()->ec2Connection()->getDiscoveryManager(owner->accessRights());
        discoveryManager->addDiscoveryInformation(
            remoteModuleInformation.id,
            simpleUrl, false,
            ec2::DummyHandler::instance(),
            &ec2::DummyHandler::onRequestDone);
    }

    nx::vms::discovery::ModuleEndpoint module(
        remoteModuleInformation, {url.host(), (uint16_t) remoteModuleInformation.port});
    owner->commonModule()->moduleDiscoveryManager()->checkEndpoint(module.endpoint, module.id);

    /* Connect to server if it is compatible */
    if (connectionResult == Qn::SuccessConnectionResult && QnServerConnector::instance())
        QnServerConnector::instance()->addConnection(module);

    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(owner->authSession(), Qn::AR_SystemmMerge);
    qnAuditManager->addAuditRecord(auditRecord);

    return nx_http::StatusCode::ok;
}

bool QnMergeSystemsRestHandler::applyCurrentSettings(
    const nx::utils::Url &remoteUrl,
    const QString& /*getKey*/,
    const QString& postKey,
    bool oneServer,
    const QnRestConnectionProcessor* owner)
{
    auto server = owner->resourcePool()->getResourceById<QnMediaServerResource>(owner->commonModule()->moduleGUID());
    if (!server)
        return false;
    Q_ASSERT(!server->getAuthKey().isEmpty());

    ConfigureSystemData data;
    data.localSystemId = owner->commonModule()->globalSettings()->localSystemId();
    data.sysIdTime = owner->commonModule()->systemIdentityTime();
    ec2::AbstractECConnectionPtr ec2Connection = owner->commonModule()->ec2Connection();
    data.tranLogTime = ec2Connection->getTransactionLogTime();
    data.wholeSystem = !oneServer;

    /**
     * Save current server to the foreign system.
     * It could be only way to pass authentication if current admin user is disabled
     */
    fromResourceToApi(server, data.foreignServer);

    /**
     * Save current admin and cloud users to the foreign system
     */
    for (const auto& user: owner->resourcePool()->getResources<QnUserResource>())
    {
        if (user->isCloud() || user->isBuiltInAdmin())
        {
            ec2::ApiUserData apiUser;
            fromResourceToApi(user, apiUser);
            data.foreignUsers.push_back(apiUser);

            for (const auto& param: user->params())
                data.additionParams.push_back(param);
        }
    }

    /**
     * Save current system settings to the foreign system.
     */
    const auto& settings = owner->commonModule()->globalSettings()->allSettings();
    for (QnAbstractResourcePropertyAdaptor* setting : settings)
    {
        ec2::ApiResourceParamData param(setting->key(), setting->serializedValue());
        data.foreignSettings.push_back(param);
    }

    if (!executeRemoteConfigure(data, remoteUrl, postKey, owner))
        return false;

    return true;
}

bool QnMergeSystemsRestHandler::executeRemoteConfigure(
    const ConfigureSystemData& data,
    const nx::utils::Url &remoteUrl,
    const QString& postKey,
    const QnRestConnectionProcessor* owner)
{
    QByteArray serializedData = QJson::serialized(data);

    nx_http::HttpClient client;
    client.setResponseReadTimeoutMs(kRequestTimeout.count());
    client.setSendTimeoutMs(kRequestTimeout.count());
    client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());
    client.addAdditionalHeader(Qn::AUTH_SESSION_HEADER_NAME, owner->authSession().toByteArray());

    nx::utils::Url requestUrl(remoteUrl);
    requestUrl.setPath(lit("/api/configure"));
    addAuthToRequest(requestUrl, postKey);
    if (!client.doPost(requestUrl, "application/json", serializedData) ||
        !isResponseOK(client))
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::executeRemoteConfigure api/configure failed. HTTP code %1.")
            .arg(client.response() ? client.response()->statusLine.statusCode : 0), cl_logWARNING);
        return false;
    }

    nx_http::BufferType response;
    while (!client.eof())
        response.append(client.fetchMessageBodyBuffer());

    QnJsonRestResult jsonResult;
    if (!QJson::deserialize(response, &jsonResult))
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::executeRemoteConfigure api/configure failed."
            "Invalid json response received."), cl_logWARNING);
        return false;
    }
    if (jsonResult.error != QnRestResult::NoError)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::executeRemoteConfigure api/configure failed. Json error %1.")
            .arg(jsonResult.error), cl_logWARNING);
        return false;
    }

    return true;
}

template <class ResultDataType>
bool executeRequest(
    const nx::utils::Url &remoteUrl,
    const QString& getKey,
    ResultDataType& result,
    const QString& path)
{
    nx_http::HttpClient client;
    client.setResponseReadTimeoutMs(kRequestTimeout.count());
    client.setSendTimeoutMs(kRequestTimeout.count());
    client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());

    nx::utils::Url requestUrl(remoteUrl);
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

    nx_http::BufferType response;
    while (!client.eof())
        response.append(client.fetchMessageBodyBuffer());

    return QJson::deserialize(response, &result);
}

bool QnMergeSystemsRestHandler::applyRemoteSettings(
    const nx::utils::Url& remoteUrl,
    const QnUuid& systemId,
    const QString& systemName,
    const QString& getKey,
    const QString& postKey,
    const QnRestConnectionProcessor* owner)
{
    /* Read admin user from the remote server */

    ec2::ApiUserDataList users;
    if (!executeRequest(remoteUrl, getKey, users, lit("/ec2/getUsers")))
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


    // 1. update settings in remove database to ensure they have priority while merge
    {
        ConfigureSystemData data;
        data.localSystemId = systemId;
        data.wholeSystem = true;
        data.sysIdTime = owner->commonModule()->systemIdentityTime();
        ec2::AbstractECConnectionPtr ec2Connection = owner->commonModule()->ec2Connection();
        data.tranLogTime = ec2Connection->getTransactionLogTime();
        data.rewriteLocalSettings = true;

        if (!executeRemoteConfigure(data, remoteUrl, postKey, owner))
            return false;

    }

    // 2. update local data
    ConfigureSystemData data;
    data.localSystemId = systemId;
    data.wholeSystem = true;
    data.sysIdTime = pingReply.sysIdTime;
    data.tranLogTime = pingReply.tranLogTime;
    data.systemName = systemName;

    for (const auto& userData: users)
    {
        QnUserResourcePtr user = fromApiToResource(userData);
        if (user->isCloud() || user->isBuiltInAdmin())
        {
            data.foreignUsers.push_back(userData);
            for (const auto& param: user->params())
                data.additionParams.push_back(param);
        }
    }

    if (!changeLocalSystemId(data, m_messageBus))
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to change system name"), cl_logDEBUG1);
        return false;
    }

    // put current server info to a foreign system to allow authorization via server key
    {
        QnMediaServerResourcePtr mServer = owner->resourcePool()->getResourceById<QnMediaServerResource>(owner->commonModule()->moduleGUID());
        if (!mServer)
            return false;
        ec2::ApiMediaServerData currentServer;
        fromResourceToApi(mServer, currentServer);

        nx_http::HttpClient client;
        client.setResponseReadTimeoutMs(kRequestTimeout.count());
        client.setSendTimeoutMs(kRequestTimeout.count());
        client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());
        client.addAdditionalHeader(Qn::AUTH_SESSION_HEADER_NAME, owner->authSession().toByteArray());

        QByteArray serializedData = QJson::serialized(currentServer);
        nx::utils::Url requestUrl(remoteUrl);
        addAuthToRequest(requestUrl, postKey);
        requestUrl.setPath(lit("/ec2/saveMediaServer"));
        if (!client.doPost(requestUrl, "application/json", serializedData) ||
            !isResponseOK(client))
        {
            return false;
        }
    }

    auto miscManager = owner->commonModule()->ec2Connection()->getMiscManager(Qn::kSystemAccess);
    ec2::ErrorCode errorCode = miscManager->changeSystemIdSync(systemId, pingReply.sysIdTime, pingReply.tranLogTime);
    NX_ASSERT(errorCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("QnMergeSystemsRestHandler::applyRemoteSettings. Failed to save new system name: %1")
            .arg(ec2::toString(errorCode)), cl_logDEBUG1);
        return false;
    }

    return true;
}

void QnMergeSystemsRestHandler::setMergeError(
    QnJsonRestResult& result,
    utils::MergeSystemsStatus::Value mergeStatus)
{
    result.setError(
        QnJsonRestResult::CantProcessRequest,
        utils::MergeSystemsStatus::toString(mergeStatus));
}
