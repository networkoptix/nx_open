#include "system_merge_processor.h"

#include <chrono>

#include <nx/fusion/serialization/json.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>

#include <api/global_settings.h>
#include <api/mediaserver_client.h>
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
#include <utils/common/app_info.h>

#include "vms_utils.h"

namespace nx {
namespace vms {
namespace utils {

namespace {

using MergeStatus = ::utils::MergeSystemsStatus::Value;

// Minimal server version which could be configured.
static const QnSoftwareVersion kMinimalVersion(2, 3);

static const std::chrono::milliseconds kRequestTimeout = std::chrono::minutes(1);

} // namespace

SystemMergeProcessor::SystemMergeProcessor(QnCommonModule* commonModule):
    m_commonModule(commonModule)
{
}

void SystemMergeProcessor::enableDbBackup(const QString& dataDirectory)
{
    m_dbBackupEnabled = true;
    m_dataDirectory = dataDirectory;
}

nx::network::http::StatusCode::Value SystemMergeProcessor::merge(
    Qn::UserAccessData accessRights,
    const QnAuthSession& authSession,
    MergeSystemData data,
    QnJsonRestResult* result)
{
    m_authSession = authSession;

    if (data.mergeOneServer)
        data.takeRemoteSettings = false;

    if (!validateInputData(data, result))
        return nx::network::http::StatusCode::badRequest;

    const nx::utils::Url url(data.url);

    saveBackupOfSomeLocalData();

    // Get module information to get system name.
    auto statusCode = fetchModuleInformation(url, data.getKey, &m_remoteModuleInformation);
    if (!nx::network::http::StatusCode::isSuccessCode(statusCode))
    {
        if (statusCode == nx::network::http::StatusCode::unauthorized)
            setMergeError(result, MergeStatus::unauthorized);
        else
            setMergeError(result, MergeStatus::notFound);
        return nx::network::http::StatusCode::serviceUnavailable;
    }

    result->setReply(m_remoteModuleInformation);

    statusCode = checkWhetherMergeIsPossible(data, result);
    if (statusCode != nx::network::http::StatusCode::ok)
        return statusCode;

    statusCode = mergeSystems(accessRights, data, result);
    if (statusCode != nx::network::http::StatusCode::ok)
        return statusCode;

    if (!addMergeHistoryRecord(data))
        return nx::network::http::StatusCode::internalServerError;

    return nx::network::http::StatusCode::ok;
}

void SystemMergeProcessor::saveBackupOfSomeLocalData()
{
    m_localModuleInformation = m_commonModule->moduleInformation();
    m_cloudAuthKey = m_commonModule->globalSettings()->cloudAuthKey().toUtf8();
}

const QnModuleInformationWithAddresses& SystemMergeProcessor::remoteModuleInformation() const
{
    return m_remoteModuleInformation;
}

bool SystemMergeProcessor::validateInputData(
    const MergeSystemData& data,
    QnJsonRestResult* result)
{
    if (data.url.isEmpty())
    {
        NX_LOG(lit("SystemMergeProcessor. Request missing required parameter \"url\""), cl_logDEBUG1);
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("url")));
        return false;
    }

    const nx::utils::Url url(data.url);
    if (!url.isValid())
    {
        NX_LOG(lit("SystemMergeProcessor. Received invalid parameter url %1")
            .arg(data.url), cl_logDEBUG1);
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::InvalidParameter, lit("url")));
        return false;
    }

    if (data.getKey.isEmpty())
    {
        NX_LOG(lit("SystemMergeProcessor. Request missing required parameter \"getKey\""), cl_logDEBUG1);
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("password")));
        return false;
    }

    return true;
}

nx::network::http::StatusCode::Value SystemMergeProcessor::checkWhetherMergeIsPossible(
    const MergeSystemData& data,
    QnJsonRestResult* result)
{
    const nx::utils::Url url(data.url);

    QnUserResourcePtr adminUser = m_commonModule->resourcePool()->getAdministrator();
    if (!adminUser)
    {
        NX_LOG(lit("SystemMergeProcessor. Failed to find admin user"), cl_logDEBUG1);
        return nx::network::http::StatusCode::internalServerError;
    }

    if (m_remoteModuleInformation.version < kMinimalVersion)
    {
        NX_LOG(lit("SystemMergeProcessor. Remote system has too old version %2 (%1)")
            .arg(data.url).arg(m_remoteModuleInformation.version.toString()), cl_logDEBUG1);
        setMergeError(result, MergeStatus::incompatibleVersion);
        return nx::network::http::StatusCode::badRequest;
    }

    const bool isLocalInCloud = !m_commonModule->globalSettings()->cloudSystemId().isEmpty();
    const bool isRemoteInCloud = !m_remoteModuleInformation.cloudSystemId.isEmpty();
    if (isLocalInCloud && isRemoteInCloud)
    {
        MediaServerClient remoteMediaServerClient(url);
        remoteMediaServerClient.setAuthenticationKey(data.getKey);
        nx::vms::api::ResourceParamDataList remoteSettings;
        const auto resultCode = remoteMediaServerClient.ec2GetSettings(&remoteSettings);
        if (resultCode != ::ec2::ErrorCode::ok)
        {
            NX_DEBUG(this, lm("Error fetching remote system settings. %1")
                .arg(::ec2::toString(resultCode)));
            setMergeError(result, MergeStatus::notFound);
            return nx::network::http::StatusCode::serviceUnavailable;
        }

        QString remoteSystemCloudOwner;
        for (const auto& remoteSetting : remoteSettings)
        {
            if (remoteSetting.name == nx::settings_names::kNameCloudAccountName)
                remoteSystemCloudOwner = remoteSetting.value;
        }

        if (remoteSystemCloudOwner != m_commonModule->globalSettings()->cloudAccountName())
        {
            NX_DEBUG(this, lm("Cannot merge two cloud systems with different owners: %1 vs %2")
                .args(m_commonModule->globalSettings()->cloudAccountName(), remoteSystemCloudOwner));
            setMergeError(result, MergeStatus::bothSystemBoundToCloud);
            return nx::network::http::StatusCode::badRequest;
        }
        else
        {
            NX_DEBUG(this, lm("Merge two cloud systems with same owner %1")
                .args(m_commonModule->globalSettings()->cloudAccountName()));
        }
    }

    bool canMerge = true;
    if (m_remoteModuleInformation.cloudSystemId != m_commonModule->globalSettings()->cloudSystemId())
    {
        if (isLocalInCloud && isRemoteInCloud)
            canMerge = true;
        else if (data.takeRemoteSettings && isLocalInCloud)
            canMerge = false;
        else if (!data.takeRemoteSettings && isRemoteInCloud)
            canMerge = false;
    }

    if (!canMerge)
    {
        NX_LOG(lit("SystemMergeProcessor (%1). Cannot merge systems bound to cloud")
            .arg(data.url), cl_logDEBUG1);
        setMergeError(result, MergeStatus::dependentSystemBoundToCloud);
        return nx::network::http::StatusCode::badRequest;
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
            .arg(nx::network::SocketGlobals::cloud().cloudHost())
            .arg(m_remoteModuleInformation.customization)
            .arg(m_remoteModuleInformation.cloudHost)
            .arg(m_remoteModuleInformation.version.toString()),
            cl_logDEBUG1);
        setMergeError(result, MergeStatus::incompatibleVersion);
        return nx::network::http::StatusCode::badRequest;
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
        return nx::network::http::StatusCode::badRequest;
    }

    QnMediaServerResourcePtr mServer =
        m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
            m_commonModule->moduleGUID());
    bool isDefaultSystemName;
    if (data.takeRemoteSettings)
        isDefaultSystemName = m_remoteModuleInformation.serverFlags.testFlag(api::SF_NewSystem);
    else
        isDefaultSystemName = mServer && (mServer->getServerFlags().testFlag(api::SF_NewSystem));
    if (isDefaultSystemName)
    {
        NX_LOG(lit("SystemMergeProcessor. Can not merge to the non configured system"), cl_logDEBUG1);
        setMergeError(result, MergeStatus::unconfiguredSystem);
        return nx::network::http::StatusCode::badRequest;
    }

    return nx::network::http::StatusCode::ok;
}

nx::network::http::StatusCode::Value SystemMergeProcessor::mergeSystems(
    Qn::UserAccessData accessRights,
    MergeSystemData data,
    QnJsonRestResult* result)
{
    if (m_dbBackupEnabled)
    {
        if (!nx::vms::utils::backupDatabase(
                m_dataDirectory,
                m_commonModule->ec2Connection()))
        {
            NX_LOG(lit("SystemMergeProcessor. takeRemoteSettings %1. Failed to backup database")
                .arg(data.takeRemoteSettings), cl_logDEBUG1);
            setMergeError(result, MergeStatus::backupFailed);
            return nx::network::http::StatusCode::internalServerError;
        }
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
            return nx::network::http::StatusCode::internalServerError;
        }
    }
    else
    {
        if (!applyCurrentSettings(
                data.url,
                data.postKey,
                data.mergeOneServer))
        {
            NX_LOG(lit("SystemMergeProcessor. takeRemoteSettings %1. Failed to apply current settings")
                .arg(data.takeRemoteSettings), cl_logDEBUG1);
            setMergeError(result, MergeStatus::configurationFailed);
            return nx::network::http::StatusCode::internalServerError;
        }
    }

    // Save additional address if needed.
    const QUrl url(data.url);
    if (!m_remoteModuleInformation.remoteAddresses.contains(url.host()))
    {
        nx::utils::Url simpleUrl;
        simpleUrl.setScheme(nx::network::http::urlSheme(m_remoteModuleInformation.sslAllowed));
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

    return nx::network::http::StatusCode::ok;
}

void SystemMergeProcessor::setMergeError(
    QnJsonRestResult* result,
    ::utils::MergeSystemsStatus::Value mergeStatus)
{
    result->setError(
        QnJsonRestResult::CantProcessRequest,
        ::utils::MergeSystemsStatus::toString(mergeStatus));
}

bool SystemMergeProcessor::applyCurrentSettings(
    const nx::utils::Url& remoteUrl,
    const QString& postKey,
    bool oneServer)
{
    auto server = m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
        m_commonModule->moduleGUID());
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
    ec2::fromResourceToApi(server, data.foreignServer);

    /**
     * Save current admin and cloud users to the foreign system
     */
    for (const auto& user : m_commonModule->resourcePool()->getResources<QnUserResource>())
    {
        if (user->isCloud() || user->isBuiltInAdmin())
        {
            nx::vms::api::UserData apiUser;
            ec2::fromResourceToApi(user, apiUser);
            data.foreignUsers.push_back(apiUser);

            for (const auto& param : user->params())
                data.additionParams.push_back(param);
        }
    }

    /**
     * Save current system settings to the foreign system.
     */
    const auto& settings = m_commonModule->globalSettings()->allSettings();
    for (QnAbstractResourcePropertyAdaptor* setting: settings)
    {
        nx::vms::api::ResourceParamData param(setting->key(), setting->serializedValue());
        data.foreignSettings.push_back(param);
    }

    if (!executeRemoteConfigure(data, remoteUrl, postKey))
        return false;

    return true;
}

bool SystemMergeProcessor::executeRemoteConfigure(
    const ConfigureSystemData& data,
    const nx::utils::Url& remoteUrl,
    const QString& postKey)
{
    QByteArray serializedData = QJson::serialized(data);

    nx::network::http::HttpClient client;
    client.setResponseReadTimeoutMs(kRequestTimeout.count());
    client.setSendTimeoutMs(kRequestTimeout.count());
    client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());
    client.addAdditionalHeader(Qn::AUTH_SESSION_HEADER_NAME, m_authSession.toByteArray());

    nx::utils::Url requestUrl(remoteUrl);
    requestUrl.setPath(lit("/api/configure"));
    addAuthToRequest(requestUrl, postKey);
    if (!client.doPost(requestUrl, "application/json", serializedData) ||
        !isResponseOK(client))
    {
        NX_LOG(lit("SystemMergeProcessor::executeRemoteConfigure api/configure failed. HTTP code %1.")
            .arg(client.response() ? client.response()->statusLine.statusCode : 0), cl_logWARNING);
        return false;
    }

    nx::network::http::BufferType response;
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

bool SystemMergeProcessor::applyRemoteSettings(
    const nx::utils::Url& remoteUrl,
    const QnUuid& systemId,
    const QString& systemName,
    const QString& getKey,
    const QString& postKey)
{
    /* Read admin user from the remote server */

    nx::vms::api::UserDataList users;
    if (!executeRequest(remoteUrl, getKey, users, lit("/ec2/getUsers")))
        return false;

    QnJsonRestResult pingRestResult;
    if (!executeRequest(remoteUrl, getKey, pingRestResult, lit("/api/ping")))
        return false;

    QnPingReply pingReply;
    if (!QJson::deserialize(pingRestResult.reply, &pingReply))
        return false;

    if (m_dbBackupEnabled)
    {
        QnJsonRestResult backupDBRestResult;
        if (!executeRequest(remoteUrl, getKey, backupDBRestResult, lit("/api/backupDatabase")))
            return false;
    }

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
        QnUserResourcePtr user = ec2::fromApiToResource(userData);
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

    // put current server info to a foreign system to allow authorization via server key
    {
        QnMediaServerResourcePtr mServer =
            m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
                m_commonModule->moduleGUID());
        if (!mServer)
            return false;
        api::MediaServerData currentServer;
        ec2::fromResourceToApi(mServer, currentServer);

        nx::network::http::HttpClient client;
        client.setResponseReadTimeoutMs(kRequestTimeout.count());
        client.setSendTimeoutMs(kRequestTimeout.count());
        client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());
        client.addAdditionalHeader(
            Qn::AUTH_SESSION_HEADER_NAME,
            m_authSession.toByteArray());

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

bool SystemMergeProcessor::isResponseOK(const nx::network::http::HttpClient& client)
{
    if (!client.response())
        return false;
    return client.response()->statusLine.statusCode == nx::network::http::StatusCode::ok;
}

nx::network::http::StatusCode::Value SystemMergeProcessor::getClientResponse(
    const nx::network::http::HttpClient& client)
{
    if (client.response())
        return (nx::network::http::StatusCode::Value) client.response()->statusLine.statusCode;
    else
        return nx::network::http::StatusCode::undefined;
}

template <class ResultDataType>
bool SystemMergeProcessor::executeRequest(
    const nx::utils::Url& remoteUrl,
    const QString& getKey,
    ResultDataType& result,
    const QString& path)
{
    nx::network::http::HttpClient client;
    client.setResponseReadTimeoutMs(kRequestTimeout.count());
    client.setSendTimeoutMs(kRequestTimeout.count());
    client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());

    nx::utils::Url requestUrl(remoteUrl);
    requestUrl.setPath(path);
    addAuthToRequest(requestUrl, getKey);
    if (!client.doGet(requestUrl) || !isResponseOK(client))
    {
        auto status = getClientResponse(client);
        NX_LOG(lit("SystemMergeProcessor::applyRemoteSettings. Failed to invoke %1: %2")
            .arg(path)
            .arg(QLatin1String(nx::network::http::StatusCode::toString(status))), cl_logDEBUG1);
        return false;
    }

    nx::network::http::BufferType response;
    while (!client.eof())
        response.append(client.fetchMessageBodyBuffer());

    return QJson::deserialize(response, &result);
}

void SystemMergeProcessor::addAuthToRequest(
    nx::utils::Url& request,
    const QString& remoteAuthKey)
{
    QUrlQuery query(request.query());
    query.addQueryItem(QLatin1String(Qn::URL_QUERY_AUTH_KEY_NAME), remoteAuthKey);
    request.setQuery(query);
}

nx::network::http::StatusCode::Value SystemMergeProcessor::fetchModuleInformation(
    const nx::utils::Url& url,
    const QString& authenticationKey,
    QnModuleInformationWithAddresses* moduleInformation)
{
    QByteArray moduleInformationData;
    {
        nx::network::http::HttpClient client;
        client.setResponseReadTimeoutMs(kRequestTimeout.count());
        client.setSendTimeoutMs(kRequestTimeout.count());
        client.setMessageBodyReadTimeoutMs(kRequestTimeout.count());

        QUrlQuery query;
        query.addQueryItem(lit("checkOwnerPermissions"), lit("true"));
        query.addQueryItem(lit("showAddresses"), lit("true"));

        nx::utils::Url requestUrl(url);
        requestUrl.setPath(lit("/api/moduleInformationAuthenticated"));
        requestUrl.setQuery(query);
        addAuthToRequest(requestUrl, authenticationKey);

        if (!client.doGet(requestUrl) || !isResponseOK(client))
        {
            auto status = getClientResponse(client);
            NX_LOG(lm("SystemMergeProcessor. Error requesting url %1: %2")
                .args(url, nx::network::http::StatusCode::toString(status)), cl_logDEBUG1);
            return status == nx::network::http::StatusCode::undefined
                ? nx::network::http::StatusCode::serviceUnavailable
                : status;
        }
        /* if we've got it successfully we know system name and admin password */
        while (!client.eof())
            moduleInformationData.append(client.fetchMessageBodyBuffer());
    }

    const auto json = QJson::deserialized<QnJsonRestResult>(moduleInformationData);
    *moduleInformation = json.deserialized<QnModuleInformationWithAddresses>();

    return nx::network::http::StatusCode::ok;
}

bool SystemMergeProcessor::addMergeHistoryRecord(const MergeSystemData& data)
{
    const auto& mergedSystemModuleInformation = data.takeRemoteSettings
        ? m_localModuleInformation
        : static_cast<const QnModuleInformation&>(m_remoteModuleInformation);

    nx::vms::api::SystemMergeHistoryRecord mergeHistoryRecord;
    mergeHistoryRecord.timestamp = QDateTime::currentMSecsSinceEpoch();
    mergeHistoryRecord.mergedSystemLocalId =
        mergedSystemModuleInformation.localSystemId.toSimpleByteArray();
    mergeHistoryRecord.username = m_authSession.userName;
    if (!mergedSystemModuleInformation.cloudSystemId.isEmpty())
    {
        mergeHistoryRecord.mergedSystemCloudId =
            mergedSystemModuleInformation.cloudSystemId.toUtf8();
        if (mergedSystemModuleInformation.cloudSystemId == m_localModuleInformation.cloudSystemId)
            mergeHistoryRecord.sign(m_cloudAuthKey);
    }

    auto miscManager = m_commonModule->ec2Connection()->getMiscManager(Qn::kSystemAccess);
    const auto errorCode = miscManager->saveSystemMergeHistoryRecord(mergeHistoryRecord);
    if (errorCode != ::ec2::ErrorCode::ok)
    {
        NX_DEBUG(this, lm("Failed to save merge history record. %1")
            .arg(::ec2::toString(errorCode)));
        return false;
    }

    return true;
}

} // namespace utils
} // namespace vms
} // namespace nx
