#include "server_merge_processor.h"

#include <chrono>

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/log/log.h>

#include <api/global_settings.h>
#include <api/model/ping_reply.h>
#include <api/resource_property_adaptor.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <network/connection_validator.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/dummy_handler.h>
#include <rest/server/json_rest_result.h>

#include "vms_utils.h"

namespace nx {
namespace vms {
namespace utils {

namespace {

using MergeStatus = ::utils::MergeSystemsStatus::Value;

// Minimal server version which could be configured.
static const QnSoftwareVersion kMinimalVersion(2, 3);

static const std::chrono::milliseconds kRequestTimeout = std::chrono::minutes(1);

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

} // namespace

SystemMergeProcessor::SystemMergeProcessor(
    QnCommonModule* commonModule,
    const QString& dataDirectory)
    :
    m_commonModule(commonModule),
    m_dataDirectory(dataDirectory)
{
}

nx_http::StatusCode::Value SystemMergeProcessor::merge(
    Qn::UserAccessData accessRights,
    const QnAuthSession& authSession,
    MergeSystemData data,
    QnJsonRestResult& result)
{
    m_authSession = authSession;

    if (data.mergeOneServer)
        data.takeRemoteSettings = false;

    if (data.url.isEmpty())
    {
        NX_LOG(lit("SystemMergeProcessor. Request missing required parameter \"url\""), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("url")));
        return nx_http::StatusCode::badRequest;
    }

    const QUrl url(data.url);
    if (!url.isValid())
    {
        NX_LOG(lit("SystemMergeProcessor. Received invalid parameter url %1")
            .arg(data.url), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::InvalidParameter, lit("url")));
        return nx_http::StatusCode::badRequest;
    }

    if (data.getKey.isEmpty())
    {
        NX_LOG(lit("SystemMergeProcessor. Request missing required parameter \"getKey\""), cl_logDEBUG1);
        result.setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("password")));
        return nx_http::StatusCode::badRequest;
    }

    /* Get module information to get system name. */
    QnUserResourcePtr adminUser = m_commonModule->resourcePool()->getAdministrator();
    if (!adminUser)
    {
        NX_LOG(lit("SystemMergeProcessor. Failed to find admin user"), cl_logDEBUG1);
        return nx_http::StatusCode::internalServerError;
    }

    QByteArray moduleInformationData;
    {
        nx_http::HttpClient client;
        client.setResponseReadTimeoutMs(kRequestTimeout.count());
        client.setSendTimeoutMs(kRequestTimeout.count());
        client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());

        QUrlQuery query;
        query.addQueryItem(lit("checkOwnerPermissions"), lit("true"));
        query.addQueryItem(lit("showAddresses"), lit("true"));

        QUrl requestUrl(url);
        requestUrl.setPath(lit("/api/moduleInformationAuthenticated"));
        requestUrl.setQuery(query);
        addAuthToRequest(requestUrl, data.getKey);

        if (!client.doGet(requestUrl) || !isResponseOK(client))
        {
            auto status = getClientResponse(client);
            NX_LOG(lit("SystemMergeProcessor. Error requesting url %1: %2")
                .arg(data.url).arg(QLatin1String(nx_http::StatusCode::toString(status))),
                cl_logDEBUG1);
            if (status == nx_http::StatusCode::unauthorized)
                setMergeError(result, MergeStatus::unauthorized);
            else if (status == nx_http::StatusCode::unauthorized)
                setMergeError(result, MergeStatus::forbidden);
            else
                setMergeError(result, MergeStatus::notFound);
            return nx_http::StatusCode::serviceUnavailable;
        }
        /* if we've got it successfully we know system name and admin password */
        while (!client.eof())
            moduleInformationData.append(client.fetchMessageBodyBuffer());
    }

    const auto json = QJson::deserialized<QnJsonRestResult>(moduleInformationData);
    m_remoteModuleInformation = json.deserialized<QnModuleInformationWithAddresses>();

    result.setReply(m_remoteModuleInformation);

    if (m_remoteModuleInformation.version < kMinimalVersion)
    {
        NX_LOG(lit("SystemMergeProcessor. Remote system has too old version %2 (%1)")
            .arg(data.url).arg(m_remoteModuleInformation.version.toString()), cl_logDEBUG1);
        setMergeError(result, MergeStatus::incompatibleVersion);
        return nx_http::StatusCode::badRequest;
    }

    bool isLocalInCloud = !m_commonModule->globalSettings()->cloudSystemId().isEmpty();
    bool isRemoteInCloud = !m_remoteModuleInformation.cloudSystemId.isEmpty();
    if (isLocalInCloud && isRemoteInCloud)
    {
        NX_LOG(lit("SystemMergeProcessor (%1). Cannot merge because both systems are bound to the cloud")
            .arg(data.url), cl_logDEBUG1);
        setMergeError(result, MergeStatus::bothSystemBoundToCloud);
        return nx_http::StatusCode::badRequest;
    }

    bool canMerge = true;
    if (m_remoteModuleInformation.cloudSystemId != m_commonModule->globalSettings()->cloudSystemId())
    {
        if (data.takeRemoteSettings && isLocalInCloud)
            canMerge = false;
        else if (!data.takeRemoteSettings && isRemoteInCloud)
            canMerge = false;
    }

    if (!canMerge)
    {
        NX_LOG(lit("SystemMergeProcessor (%1). Cannot merge systems bound to cloud")
            .arg(data.url), cl_logDEBUG1);
        setMergeError(result, MergeStatus::dependentSystemBoundToCloud);
        return nx_http::StatusCode::badRequest;
    }

    const auto connectionResult = QnConnectionValidator::validateConnection(m_remoteModuleInformation);
    if (connectionResult == Qn::IncompatibleInternalConnectionResult
        || connectionResult == Qn::IncompatibleCloudHostConnectionResult
        || connectionResult == Qn::IncompatibleVersionConnectionResult)
    {
        NX_LOG(lit("SystemMergeProcessor. Incompatible systems. "
            "Local customization %1, cloud host %2, "
            "remote customization %3, cloud host %4, version %5")
            .arg(QnAppInfo::customizationName())
            .arg(nx::network::AppInfo::defaultCloudHost())
            .arg(m_remoteModuleInformation.customization)
            .arg(m_remoteModuleInformation.cloudHost)
            .arg(m_remoteModuleInformation.version.toString()),
            cl_logDEBUG1);
        setMergeError(result, MergeStatus::incompatibleVersion);
        return nx_http::StatusCode::badRequest;
    }

    if (connectionResult == Qn::IncompatibleProtocolConnectionResult
        && !data.ignoreIncompatible)
    {
        NX_LOG(lit("SystemMergeProcessor. Incompatible systems protocol. "
            "Local %1, remote %2")
            .arg(QnAppInfo::ec2ProtoVersion())
            .arg(m_remoteModuleInformation.protoVersion),
            cl_logDEBUG1);
        setMergeError(result, MergeStatus::incompatibleVersion);
        return nx_http::StatusCode::badRequest;
    }

    QnMediaServerResourcePtr mServer = 
        m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
            m_commonModule->moduleGUID());
    bool isDefaultSystemName;
    if (data.takeRemoteSettings)
        isDefaultSystemName = m_remoteModuleInformation.serverFlags.testFlag(Qn::SF_NewSystem);
    else
        isDefaultSystemName = mServer && (mServer->getServerFlags().testFlag(Qn::SF_NewSystem));
    if (isDefaultSystemName)
    {
        NX_LOG(lit("SystemMergeProcessor. Can not merge to the non configured system"), cl_logDEBUG1);
        setMergeError(result, MergeStatus::unconfiguredSystem);
        return nx_http::StatusCode::badRequest;
    }

    if (!nx::vms::utils::backupDatabase(
            m_dataDirectory,
            m_commonModule->ec2Connection()))
    {
        NX_LOG(lit("SystemMergeProcessor. takeRemoteSettings %1. Failed to backup database")
            .arg(data.takeRemoteSettings), cl_logDEBUG1);
        setMergeError(result, MergeStatus::backupFailed);
        return nx_http::StatusCode::internalServerError;
    }

    if (data.takeRemoteSettings)
    {
        if (!applyRemoteSettings(
                data.url,
                m_remoteModuleInformation.localSystemId,
                m_remoteModuleInformation.systemName,
                data.getKey,
                data.postKey))
        {
            NX_LOG(lit("SystemMergeProcessor. takeRemoteSettings %1. Failed to apply remote settings")
                .arg(data.takeRemoteSettings), cl_logDEBUG1);
            setMergeError(result, MergeStatus::configurationFailed);
            return nx_http::StatusCode::internalServerError;
        }
    }
    else
    {
        if (!applyCurrentSettings(
                data.url,
                data.getKey,
                data.postKey,
                data.mergeOneServer))
        {
            NX_LOG(lit("SystemMergeProcessor. takeRemoteSettings %1. Failed to apply current settings")
                .arg(data.takeRemoteSettings), cl_logDEBUG1);
            setMergeError(result, MergeStatus::configurationFailed);
            return nx_http::StatusCode::internalServerError;
        }
    }

    /* Save additional address if needed */
    if (!m_remoteModuleInformation.remoteAddresses.contains(url.host()))
    {
        QUrl simpleUrl;
        simpleUrl.setScheme(lit("http"));
        simpleUrl.setHost(url.host());
        if (url.port() != m_remoteModuleInformation.port)
            simpleUrl.setPort(url.port());
        auto discoveryManager = m_commonModule->ec2Connection()->getDiscoveryManager(accessRights);
        discoveryManager->addDiscoveryInformation(
            m_remoteModuleInformation.id,
            simpleUrl, false,
            ec2::DummyHandler::instance(),
            &ec2::DummyHandler::onRequestDone);
    }

    return nx_http::StatusCode::ok;
}

const QnModuleInformationWithAddresses& SystemMergeProcessor::remoteModuleInformation() const
{
    return m_remoteModuleInformation;
}

void SystemMergeProcessor::setMergeError(
    QnJsonRestResult& result,
    ::utils::MergeSystemsStatus::Value mergeStatus)
{
    result.setError(
        QnJsonRestResult::CantProcessRequest,
        ::utils::MergeSystemsStatus::toString(mergeStatus));
}

bool SystemMergeProcessor::applyCurrentSettings(
    const QUrl &remoteUrl,
    const QString& /*getKey*/,
    const QString& postKey,
    bool oneServer)
{
    auto server = m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(m_commonModule->moduleGUID());
    if (!server)
        return false;
    Q_ASSERT(!server->getAuthKey().isEmpty());

    ConfigureSystemData data;
    data.localSystemId = m_commonModule->globalSettings()->localSystemId();
    data.sysIdTime = m_commonModule->systemIdentityTime();
    ec2::AbstractECConnectionPtr ec2Connection = m_commonModule->ec2Connection();
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
    for (const auto& user : m_commonModule->resourcePool()->getResources<QnUserResource>())
    {
        if (user->isCloud() || user->isBuiltInAdmin())
        {
            ec2::ApiUserData apiUser;
            fromResourceToApi(user, apiUser);
            data.foreignUsers.push_back(apiUser);

            for (const auto& param : user->params())
                data.additionParams.push_back(param);
        }
    }

    /**
    * Save current system settings to the foreign system.
    */
    const auto& settings = m_commonModule->globalSettings()->allSettings();
    for (QnAbstractResourcePropertyAdaptor* setting : settings)
    {
        ec2::ApiResourceParamData param(setting->key(), setting->serializedValue());
        data.foreignSettings.push_back(param);
    }

    if (!executeRemoteConfigure(data, remoteUrl, postKey))
        return false;

    return true;
}

bool SystemMergeProcessor::executeRemoteConfigure(
    const ConfigureSystemData& data,
    const QUrl &remoteUrl,
    const QString& postKey)
{
    QByteArray serializedData = QJson::serialized(data);

    nx_http::HttpClient client;
    client.setResponseReadTimeoutMs(kRequestTimeout.count());
    client.setSendTimeoutMs(kRequestTimeout.count());
    client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());
    client.addAdditionalHeader(Qn::AUTH_SESSION_HEADER_NAME, m_authSession.toByteArray());

    QUrl requestUrl(remoteUrl);
    requestUrl.setPath(lit("/api/configure"));
    addAuthToRequest(requestUrl, postKey);
    if (!client.doPost(requestUrl, "application/json", serializedData) ||
        !isResponseOK(client))
    {
        NX_LOG(lit("SystemMergeProcessor::executeRemoteConfigure api/configure failed. HTTP code %1.")
            .arg(client.response() ? client.response()->statusLine.statusCode : 0), cl_logWARNING);
        return false;
    }

    nx_http::BufferType response;
    while (!client.eof())
        response.append(client.fetchMessageBodyBuffer());

    QnJsonRestResult jsonResult;
    if (!QJson::deserialize(response, &jsonResult))
    {
        NX_LOG(lit("SystemMergeProcessor::executeRemoteConfigure api/configure failed."
            "Invalid json response received."), cl_logWARNING);
        return false;
    }
    if (jsonResult.error != QnRestResult::NoError)
    {
        NX_LOG(lit("SystemMergeProcessor::executeRemoteConfigure api/configure failed. Json error %1.")
            .arg(jsonResult.error), cl_logWARNING);
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
    client.setResponseReadTimeoutMs(kRequestTimeout.count());
    client.setSendTimeoutMs(kRequestTimeout.count());
    client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());

    QUrl requestUrl(remoteUrl);
    requestUrl.setPath(path);
    addAuthToRequest(requestUrl, getKey);
    if (!client.doGet(requestUrl) || !isResponseOK(client))
    {
        auto status = getClientResponse(client);
        NX_LOG(lit("SystemMergeProcessor::applyRemoteSettings. Failed to invoke %1: %2")
            .arg(path)
            .arg(QLatin1String(nx_http::StatusCode::toString(status))), cl_logDEBUG1);
        return false;
    }

    nx_http::BufferType response;
    while (!client.eof())
        response.append(client.fetchMessageBodyBuffer());

    return QJson::deserialize(response, &result);
}

bool SystemMergeProcessor::applyRemoteSettings(
    const QUrl& remoteUrl,
    const QnUuid& systemId,
    const QString& systemName,
    const QString& getKey,
    const QString& postKey)
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
        data.sysIdTime = m_commonModule->systemIdentityTime();
        ec2::AbstractECConnectionPtr ec2Connection = m_commonModule->ec2Connection();
        data.tranLogTime = ec2Connection->getTransactionLogTime();
        data.rewriteLocalSettings = true;

        if (!executeRemoteConfigure(data, remoteUrl, postKey))
            return false;

    }

    // 2. update local data
    ConfigureSystemData data;
    data.localSystemId = systemId;
    data.wholeSystem = true;
    data.sysIdTime = pingReply.sysIdTime;
    data.tranLogTime = pingReply.tranLogTime;
    data.systemName = systemName;

    for (const auto& userData : users)
    {
        QnUserResourcePtr user = fromApiToResource(userData);
        if (user->isCloud() || user->isBuiltInAdmin())
        {
            data.foreignUsers.push_back(userData);
            for (const auto& param : user->params())
                data.additionParams.push_back(param);
        }
    }

    if (!nx::vms::utils::configureLocalPeerAsPartOfASystem(m_commonModule, data))
    {
        NX_LOG(lit("SystemMergeProcessor::applyRemoteSettings. Failed to change system name"), cl_logDEBUG1);
        return false;
    }

    // TODO: #ak nx::ServerSetting::setAuthKey.

    // put current server info to a foreign system to allow authorization via server key
    {
        QnMediaServerResourcePtr mServer = 
            m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
                m_commonModule->moduleGUID());
        if (!mServer)
            return false;
        ec2::ApiMediaServerData currentServer;
        fromResourceToApi(mServer, currentServer);

        nx_http::HttpClient client;
        client.setResponseReadTimeoutMs(kRequestTimeout.count());
        client.setSendTimeoutMs(kRequestTimeout.count());
        client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());
        client.addAdditionalHeader(
            Qn::AUTH_SESSION_HEADER_NAME,
            m_authSession.toByteArray());

        QByteArray serializedData = QJson::serialized(currentServer);
        QUrl requestUrl(remoteUrl);
        addAuthToRequest(requestUrl, postKey);
        requestUrl.setPath(lit("/ec2/saveMediaServer"));
        if (!client.doPost(requestUrl, "application/json", serializedData) ||
            !isResponseOK(client))
        {
            return false;
        }
    }

    auto miscManager = m_commonModule->ec2Connection()->getMiscManager(Qn::kSystemAccess);
    ec2::ErrorCode errorCode = miscManager->changeSystemIdSync(systemId, pingReply.sysIdTime, pingReply.tranLogTime);
    NX_ASSERT(errorCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("SystemMergeProcessor::applyRemoteSettings. Failed to save new system name: %1")
            .arg(ec2::toString(errorCode)), cl_logDEBUG1);
        return false;
    }

    return true;
}

} // namespace utils
} // namespace vms
} // namespace nx
