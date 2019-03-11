#include "system_merge_processor.h"

#include <chrono>

#include <nx/fusion/serialization/json.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_client.h>
#include <nx/network/socket_global.h>
#include <nx/utils/counter.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/algorithm.h>

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

#include <nx/vms/utils/vms_utils.h>
#include "system_helpers.h"

namespace nx {
namespace vms {
namespace utils {

namespace {

using MergeStatus = ::utils::MergeSystemsStatus::Value;

// Minimal server version which could be configured.
static const nx::utils::SoftwareVersion kMinimalVersion = {2, 3};

static const std::chrono::milliseconds kRequestTimeout = std::chrono::minutes(1);

} // namespace

SystemMergeProcessor::SystemMergeProcessor(QnCommonModule* commonModule):
    m_commonModule(commonModule)
{
}

void SystemMergeProcessor::enableDbBackup(const QString& backupDirectory)
{
    m_dbBackupEnabled = true;
    m_backupDirectory = backupDirectory;
}

QnJsonRestResult SystemMergeProcessor::merge(
    Qn::UserAccessData accessRights,
    const QnAuthSession& authSession,
    MergeSystemData data)
{
    NX_DEBUG(this, "Merge. %1", QJson::serialized(data));

    m_authSession = authSession;

    if (data.mergeOneServer)
        data.takeRemoteSettings = false;

    QnJsonRestResult result;
    if (!validateInputData(data, &result))
        return result;

    const nx::utils::Url url(data.url);

    saveBackupOfSomeLocalData();

    // Get module information to get system name.
    auto statusCode = fetchModuleInformation(url, data.getKey, &m_remoteModuleInformation);
    if (!nx::network::http::StatusCode::isSuccessCode(statusCode))
    {
        NX_DEBUG(this, lm("Failed to read remote module information. %1")
            .args(nx::network::http::StatusCode::toString(statusCode)));

        if (statusCode == nx::network::http::StatusCode::unauthorized)
            setMergeError(&result, MergeStatus::unauthorized);
        else
            setMergeError(&result, MergeStatus::notFound);
        return result;
    }

    result.setReply(m_remoteModuleInformation);

    result = checkWhetherMergeIsPossible(data);
    if (result.error)
    {
        NX_DEBUG(this, lm("Systems cannot be merged"));
        return result;
    }

    result = mergeSystems(accessRights, data);
    if (result.error)
    {
        NX_DEBUG(this, lm("Failed to merge systems. %1")
            .args(toString(result.error)));
        return result;
    }

    nx::vms::api::SystemMergeHistoryRecord mergeHistoryRecord;
    if (!addMergeHistoryRecord(data, &mergeHistoryRecord))
    {
        NX_DEBUG(this, "Failed to add merge history data");
        setMergeError(&result, MergeStatus::unknownError);
        return result;
    }
    QJson::serialize(mergeHistoryRecord, &result.reply);

    NX_DEBUG(this, "Merge succeeded");
    return result;
}

void SystemMergeProcessor::saveBackupOfSomeLocalData()
{
    m_localModuleInformation = m_commonModule->moduleInformation();
    m_cloudAuthKey = m_commonModule->globalSettings()->cloudAuthKey().toUtf8();
}

const nx::vms::api::ModuleInformationWithAddresses&
    SystemMergeProcessor::remoteModuleInformation() const
{
    return m_remoteModuleInformation;
}

bool SystemMergeProcessor::validateInputData(
    const MergeSystemData& data,
    QnJsonRestResult* result)
{
    if (data.url.isEmpty())
    {
        NX_DEBUG(this, lit("Request missing required parameter \"url\""));
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("url")));
        return false;
    }

    const nx::utils::Url url(data.url);
    if (!url.isValid())
    {
        NX_DEBUG(this, lit("Received invalid parameter url %1")
            .arg(data.url));
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::InvalidParameter, lit("url")));
        return false;
    }

    if (data.getKey.isEmpty())
    {
        NX_DEBUG(this, lit("Request missing required parameter \"getKey\""));
        result->setError(QnRestResult::ErrorDescriptor(
            QnJsonRestResult::MissingParameter, lit("password")));
        return false;
    }

    return true;
}

QnJsonRestResult SystemMergeProcessor::checkWhetherMergeIsPossible(
    const MergeSystemData& data)
{
    QnJsonRestResult result;
    const nx::utils::Url remoteServerUrl(data.url);

    QnUserResourcePtr adminUser = m_commonModule->resourcePool()->getAdministrator();
    if (!adminUser)
    {
        NX_DEBUG(this, lit("Failed to find admin user"));
        setMergeError(&result, MergeStatus::unknownError);
        return result;
    }

    if (m_remoteModuleInformation.version < kMinimalVersion)
    {
        NX_DEBUG(this, lit("Remote system has too old version %2 (%1)")
            .arg(data.url).arg(m_remoteModuleInformation.version.toString()));
        setMergeError(&result, MergeStatus::incompatibleVersion);
        return result;
    }

    MediaServerClient remoteMediaServerClient(remoteServerUrl);
    remoteMediaServerClient.setRequestTimeout(kRequestTimeout);
    remoteMediaServerClient.setAuthenticationKey(data.getKey);

    result = checkIfSystemsHaveServerWithSameId(&remoteMediaServerClient);
    if (result.error)
        return result;

    result = checkIfCloudSystemsMergeIsPossible(data, &remoteMediaServerClient);
    if (result.error)
        return result;

    const auto connectionResult = QnConnectionValidator::validateConnection(m_remoteModuleInformation);
    if (connectionResult == Qn::IncompatibleInternalConnectionResult
        || connectionResult == Qn::IncompatibleCloudHostConnectionResult
        || connectionResult == Qn::IncompatibleVersionConnectionResult)
    {
        NX_DEBUG(this, lm("Incompatible systems. Local customization %1, cloud host %2, "
            "remote customization %3, cloud host %4, version %5")
            .args(QnAppInfo::customizationName(),
                nx::network::SocketGlobals::cloud().cloudHost(),
                m_remoteModuleInformation.customization,
                m_remoteModuleInformation.cloudHost,
                m_remoteModuleInformation.version.toString()));
        setMergeError(&result, MergeStatus::incompatibleVersion);
        return result;
    }

    if (connectionResult == Qn::IncompatibleProtocolConnectionResult
        && !data.ignoreIncompatible)
    {
        NX_DEBUG(this, lm("Incompatible systems protocol. Local %1, remote %2")
            .args(QnAppInfo::ec2ProtoVersion(), m_remoteModuleInformation.protoVersion));
        setMergeError(&result, MergeStatus::incompatibleVersion);
        return result;
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
        NX_DEBUG(this, lit("Cannot merge to the unconfigured system"));
        setMergeError(&result, MergeStatus::unconfiguredSystem);
        return result;
    }

    return result;
}

QnJsonRestResult SystemMergeProcessor::checkIfSystemsHaveServerWithSameId(
        MediaServerClient* remoteMediaServerClient)
{
    QnJsonRestResult result;
    nx::vms::api::MediaServerDataExList remoteMediaServers;
    auto resultCode = remoteMediaServerClient->ec2GetMediaServersEx(&remoteMediaServers);
    if (resultCode != ec2::ErrorCode::ok)
    {
        NX_DEBUG(this, lm("Error fetching mediaserver list from remote system. %1")
            .args(::ec2::toString(resultCode)));
        setMergeError(&result, MergeStatus::configurationFailed);
        return result;
    }

    auto serverManager =
        m_commonModule->ec2Connection()->getMediaServerManager(Qn::kSystemAccess);
    nx::vms::api::MediaServerDataExList localMediaServers;
    resultCode = serverManager->getServersExSync(&localMediaServers);
    if (resultCode != ec2::ErrorCode::ok)
    {
        NX_DEBUG(this, lm("Error fetching local mediaserver list. %1")
            .args(::ec2::toString(resultCode)));
        setMergeError(&result, MergeStatus::configurationFailed);
        return result;
    }

    for (const auto& localMediaServer: localMediaServers)
    {
        const auto sameMserverIter = std::find_if(
            remoteMediaServers.begin(), remoteMediaServers.end(),
            [id = localMediaServer.id](const auto& ms) { return id == ms.id; });
        if (sameMserverIter != remoteMediaServers.end())
        {
            NX_DEBUG(this, lm("Merge error. Both systems have same mediaserver %1")
                .args(sameMserverIter->id));
            setMergeError(&result, MergeStatus::duplicateMediaServerFound);
            return result;
        }
    }

    return result;
}

QnJsonRestResult SystemMergeProcessor::checkIfCloudSystemsMergeIsPossible(
    const MergeSystemData& data,
    MediaServerClient* remoteMediaServerClient)
{
    QnJsonRestResult result;
    const bool isLocalInCloud = !m_commonModule->globalSettings()->cloudSystemId().isEmpty();
    const bool isRemoteInCloud = !m_remoteModuleInformation.cloudSystemId.isEmpty();
    if (isLocalInCloud && isRemoteInCloud)
    {
        nx::vms::api::ResourceParamDataList remoteSettings;
        const auto resultCode = remoteMediaServerClient->ec2GetSettings(&remoteSettings);
        if (resultCode != ::ec2::ErrorCode::ok)
        {
            NX_DEBUG(this, lm("Error fetching remote system settings. %1")
                .arg(::ec2::toString(resultCode)));
            setMergeError(&result, MergeStatus::notFound);
            return result;
        }

        QString remoteSystemCloudOwner;
        for (const auto& remoteSetting: remoteSettings)
        {
            if (remoteSetting.name == nx::settings_names::kNameCloudAccountName)
                remoteSystemCloudOwner = remoteSetting.value;
        }

        if (remoteSystemCloudOwner != m_commonModule->globalSettings()->cloudAccountName())
        {
            NX_DEBUG(this, lm("Cannot merge two cloud systems with different owners: %1 vs %2")
                .args(m_commonModule->globalSettings()->cloudAccountName(), remoteSystemCloudOwner));
            setMergeError(&result, MergeStatus::cloudSystemsHaveDifferentOwners);
            return result;
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
        NX_DEBUG(this, lit("SystemMergeProcessor (%1). Cannot merge systems bound to cloud")
            .arg(data.url));
        setMergeError(&result, MergeStatus::dependentSystemBoundToCloud);
        return result;
    }

    return result;
}

QnJsonRestResult SystemMergeProcessor::mergeSystems(
    Qn::UserAccessData accessRights,
    MergeSystemData data)
{
    QnJsonRestResult result;
    if (m_dbBackupEnabled)
    {
        NX_DEBUG(this, "Backing up the database");

        if (!nx::vms::utils::backupDatabase(
                m_backupDirectory,
                m_commonModule->ec2Connection()))
        {
            NX_DEBUG(this, lit("takeRemoteSettings %1. Failed to backup database")
                .arg(data.takeRemoteSettings));
            setMergeError(&result, MergeStatus::backupFailed);
            return result;
        }
    }

    if (data.takeRemoteSettings)
    {
        NX_DEBUG(this, "Applying remote peer settings");

        result = applyRemoteSettings(
                data.url,
                m_remoteModuleInformation.localSystemId,
                m_remoteModuleInformation.systemName,
                data.getKey,
                data.postKey);
        if (result.error)
        {
            NX_DEBUG(this, lit("takeRemoteSettings %1. Failed to apply remote settings")
                .arg(data.takeRemoteSettings));
            return result;
        }
    }
    else
    {
        NX_DEBUG(this, "Applying local settings to a remote peer");

        result = applyCurrentSettings(data.url, data.postKey, data.mergeOneServer);
        if (result.error)
        {
            NX_DEBUG(this, lit("takeRemoteSettings %1. Failed to apply current settings")
                .arg(data.takeRemoteSettings));
            return result;
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

    return result;
}

void SystemMergeProcessor::setMergeError(
    QnJsonRestResult* result,
    ::utils::MergeSystemsStatus::Value mergeStatus)
{
    result->setError(
        QnJsonRestResult::CantProcessRequest,
        ::utils::MergeSystemsStatus::toString(mergeStatus));
}

QnJsonRestResult SystemMergeProcessor::applyCurrentSettings(
    const nx::utils::Url& remoteUrl,
    const QString& postKey,
    bool oneServer)
{
    auto server = m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
        m_commonModule->moduleGUID());
    if (!server)
    {
        QnJsonRestResult result;
        setMergeError(&result, MergeStatus::configurationFailed);
        return result;
    }
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
    for (const auto& user: m_commonModule->resourcePool()->getResources<QnUserResource>())
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

    return executeRemoteConfigure(data, remoteUrl, postKey);
}

void SystemMergeProcessor::shiftSynchronizationTimestamp(
    const QnUuid& systemId,
    const ConfigureSystemData& remoteSystemData)
{
    auto newTranLogTime = std::max(
        remoteSystemData.tranLogTime,
        m_commonModule->ec2Connection()->getTransactionLogTime());
    ++newTranLogTime.sequence;

    NX_DEBUG(this, "Shifting local synchronization timestamp to %1 so that critical data is not "
        "overwritten by some en-route transactions", newTranLogTime);

    m_commonModule->ec2Connection()->setTransactionLogTime(newTranLogTime);
}

bool SystemMergeProcessor::changeSystemId(
    const QnUuid& systemId,
    const ConfigureSystemData& remoteSystemData)
{
    auto miscManager = m_commonModule->ec2Connection()->getMiscManager(Qn::kSystemAccess);
    const auto errorCode = miscManager->changeSystemIdSync(
        systemId,
        remoteSystemData.sysIdTime,
        m_commonModule->ec2Connection()->getTransactionLogTime());

    NX_ASSERT(errorCode != ec2::ErrorCode::forbidden, "Access check should be implemented before");
    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_DEBUG(this, lit("applyRemoteSettings. Failed to save new system name: %1")
            .arg(ec2::toString(errorCode)));
        return false;
    }

    return true;
}

QnJsonRestResult SystemMergeProcessor::executeRemoteConfigure(
    const ConfigureSystemData& data,
    const nx::utils::Url& remoteUrl,
    const QString& postKey)
{
    QnJsonRestResult jsonResult;

    QByteArray serializedData = QJson::serialized(data);

    nx::network::http::HttpClient client;
    client.setResponseReadTimeout(kRequestTimeout);
    client.setSendTimeout(kRequestTimeout);
    client.setMessageBodyReadTimeout(kRequestTimeout);
    client.addAdditionalHeader(Qn::AUTH_SESSION_HEADER_NAME, m_authSession.toByteArray());

    nx::utils::Url requestUrl(remoteUrl);
    requestUrl.setPath(lit("/api/configure"));
    addAuthToRequest(requestUrl, postKey);
    if (!client.doPost(requestUrl, "application/json", serializedData) ||
        !isResponseOK(client))
    {
        auto result = client.response()
            ? nx::network::http::StatusCode::Value(client.response()->statusLine.statusCode)
            : nx::network::http::StatusCode::internalServerError;
        NX_WARNING(this, lit("executeRemoteConfigure api/configure failed. HTTP code %1.")
            .arg(result));
        setMergeError(&jsonResult, MergeStatus::configurationFailed);
        return jsonResult;
    }

    nx::network::http::BufferType response;
    while (!client.eof())
        response.append(client.fetchMessageBodyBuffer());

    if (!QJson::deserialize(response, &jsonResult))
    {
        NX_WARNING(this, lit("executeRemoteConfigure api/configure failed."
            "Invalid json response received."));
        setMergeError(&jsonResult, MergeStatus::configurationFailed);
        return jsonResult;
    }
    if (jsonResult.error != QnRestResult::NoError)
    {
        NX_WARNING(this, lit("executeRemoteConfigure api/configure failed. Json error %1 (%2)")
            .arg(jsonResult.error).arg(toString(jsonResult.errorString)));
    }

    return jsonResult;
}

QnJsonRestResult SystemMergeProcessor::applyRemoteSettings(
    const nx::utils::Url& remoteUrl,
    const QnUuid& systemId,
    const QString& systemName,
    const QString& getKey,
    const QString& postKey)
{
    ConfigureSystemData remoteSystemData;
    remoteSystemData.localSystemId = systemId;
    remoteSystemData.wholeSystem = true;
    remoteSystemData.systemName = systemName;

    QnJsonRestResult result;
    if (!fetchRemoteData(remoteUrl, getKey, &remoteSystemData))
    {
        setMergeError(&result, MergeStatus::configurationFailed);
        return result;
    }

    if (m_dbBackupEnabled)
    {
        QnJsonRestResult backupDBRestResult;
        if (!executeRequest(remoteUrl, getKey, backupDBRestResult, lit("/api/backupDatabase")))
        {
            setMergeError(&result, MergeStatus::configurationFailed);
            return result;
        }
    }

    // 1. Updating settings in remote database to ensure they have priority while merging.
    {
        ConfigureSystemData data;
        data.localSystemId = systemId;
        data.wholeSystem = true;
        data.sysIdTime = m_commonModule->systemIdentityTime();
        ec2::AbstractECConnectionPtr ec2Connection = m_commonModule->ec2Connection();
        data.tranLogTime = ec2Connection->getTransactionLogTime();
        data.rewriteLocalSettings = true;

        result = executeRemoteConfigure(
            data,
            remoteUrl,
            postKey);
        if (result.error)
            return result;
    }

    shiftSynchronizationTimestamp(systemId, remoteSystemData);

    if (!changeSystemId(systemId, remoteSystemData))
    {
        setMergeError(&result, MergeStatus::configurationFailed);
        return result;
    }

    // 2. Updating local data.

    if (!nx::vms::utils::configureLocalPeerAsPartOfASystem(m_commonModule, remoteSystemData))
    {
        NX_DEBUG(this, lit("applyRemoteSettings. Failed to change system name"));
        setMergeError(&result, MergeStatus::configurationFailed);
        return result;
    }

    // Put current server info to a foreign system to allow authorization via server key.
    {
        QnMediaServerResourcePtr mServer =
            m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
                m_commonModule->moduleGUID());
        if (!mServer)
        {
            setMergeError(&result, MergeStatus::configurationFailed);
            return result;
        }
        api::MediaServerData currentServer;
        ec2::fromResourceToApi(mServer, currentServer);

        nx::network::http::HttpClient client;
        client.setResponseReadTimeout(kRequestTimeout);
        client.setSendTimeout(kRequestTimeout);
        client.setMessageBodyReadTimeout(kRequestTimeout);
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
            setMergeError(&result, MergeStatus::configurationFailed);
            return result;
        }
    }

    return result;
}

bool SystemMergeProcessor::fetchRemoteData(
    const nx::utils::Url& remoteUrl,
    const QString& getKey,
    ConfigureSystemData* data)
{
    if (!fetchUsers(remoteUrl, getKey, data))
        return false;

    QnJsonRestResult pingRestResult;
    if (!executeRequest(remoteUrl, getKey, pingRestResult, lit("/api/ping")))
        return false;

    QnPingReply pingReply;
    if (!QJson::deserialize(pingRestResult.reply, &pingReply))
        return false;

    data->sysIdTime = pingReply.sysIdTime;
    data->tranLogTime = pingReply.tranLogTime;

    return true;
}

bool SystemMergeProcessor::fetchUsers(
    const nx::utils::Url& remoteUrl,
    const QString& getKey,
    ConfigureSystemData* data)
{
    nx::vms::api::UserDataList users;
    if (!executeRequest(remoteUrl, getKey, users, lit("/ec2/getUsers")))
        return false;

    nx::vms::api::UserDataList cloudUsers;
    std::copy_if(
        users.begin(), users.end(),
        std::back_inserter(cloudUsers),
        [](const auto& user) { return user.isCloud; });

    if (!fetchUserParams(remoteUrl, getKey, cloudUsers, data))
        return false;

    for (const auto& userData: users)
    {
        QnUserResourcePtr user = ec2::fromApiToResource(userData);
        if (user->isCloud() || user->isBuiltInAdmin())
        {
            data->foreignUsers.push_back(userData);

            for (const auto& param: user->params())
                data->additionParams.push_back(param);
        }
    }

    return true;
}

bool SystemMergeProcessor::fetchUserParams(
    const nx::utils::Url& remoteUrl,
    const QString& getKey,
    const nx::vms::api::UserDataList& users,
    ConfigureSystemData* data)
{
    MediaServerClient mediaServerClient(remoteUrl);
    mediaServerClient.setRequestTimeout(kRequestTimeout);
    mediaServerClient.setAuthenticationKey(getKey);

    std::vector<std::tuple<QnUuid, ec2::ErrorCode, nx::vms::api::ResourceParamDataList>>
        getUsersParamsResults;

    nx::utils::Counter expectedResponseCount(users.size());

    for (const auto& user: users)
    {
        mediaServerClient.ec2GetResourceParams(
            user.id,
            [this, &getUsersParamsResults, &expectedResponseCount, userId = user.id](
                ec2::ErrorCode resultCode,
                nx::vms::api::ResourceParamDataList params)
            {
                getUsersParamsResults.push_back(std::make_tuple(userId, resultCode, std::move(params)));
                expectedResponseCount.decrement();
            });
    }

    expectedResponseCount.wait();

    for (const auto& [userId, resultCode, params]: getUsersParamsResults)
    {
        if (resultCode != ec2::ErrorCode::ok)
        {
            NX_DEBUG(this, "Failed to fetch user %1 params from %2. %3",
                userId, remoteUrl, resultCode);
            return false;
        }

        for (const auto& param: params)
        {
            NX_VERBOSE(this, "Fetching for resaving. user %1, name %2, value %3",
                userId, param.name, param.value);
            data->additionParams.push_back({userId, param.name, param.value});
        }

        NX_VERBOSE(this, "Fetched %1 params of user %2 from %3",
            params.size(), userId, remoteUrl);
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
    client.setResponseReadTimeout(kRequestTimeout);
    client.setSendTimeout(kRequestTimeout);
    client.setMessageBodyReadTimeout(kRequestTimeout);

    nx::utils::Url requestUrl(remoteUrl);
    requestUrl.setPath(path);
    addAuthToRequest(requestUrl, getKey);
    if (!client.doGet(requestUrl) || !isResponseOK(client))
    {
        auto status = getClientResponse(client);
        NX_DEBUG(this, lit("applyRemoteSettings. Failed to invoke %1: %2")
            .arg(path)
            .arg(QLatin1String(nx::network::http::StatusCode::toString(status))));
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
    nx::vms::api::ModuleInformationWithAddresses* moduleInformation)
{
    QByteArray moduleInformationData;
    {
        nx::network::http::HttpClient client;
        client.setResponseReadTimeout(kRequestTimeout);
        client.setSendTimeout(kRequestTimeout);
        client.setMessageBodyReadTimeout(kRequestTimeout);

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
            NX_DEBUG(this, lm("Error requesting url %1: %2")
                .args(requestUrl, nx::network::http::StatusCode::toString(status)));
            return status == nx::network::http::StatusCode::undefined
                ? nx::network::http::StatusCode::serviceUnavailable
                : status;
        }
        /* if we've got it successfully we know system name and admin password */
        while (!client.eof())
            moduleInformationData.append(client.fetchMessageBodyBuffer());
    }

    const auto json = QJson::deserialized<QnJsonRestResult>(moduleInformationData);
    *moduleInformation = json.deserialized<nx::vms::api::ModuleInformationWithAddresses>();

    return nx::network::http::StatusCode::ok;
}

bool SystemMergeProcessor::addMergeHistoryRecord(
    const MergeSystemData& data,
    nx::vms::api::SystemMergeHistoryRecord* outResult)
{
    const auto& mergedSystemModuleInformation = data.takeRemoteSettings
        ? m_localModuleInformation
        : m_remoteModuleInformation;

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
    *outResult = mergeHistoryRecord;

    return true;
}

} // namespace utils
} // namespace vms
} // namespace nx
